#include "db.h"
#include "api.h"
#include "config.h"
#include <curl/curl.h>
#include <json-c/json.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#define MAX_EXTRA_PARAMS 8
#define BASE_PARAM_COUNT 7

char *url_encode(const char *str) {
    if (!str) return strdup("");
    size_t len = strlen(str);
    char *out = malloc(len * 3 + 1);
    char *p = out;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            *p++ = c;
        } else {
            sprintf(p, "%%%02X", c);
            p += 3;
        }
    }
    *p = '\0';
    return out;
}

char *base64_encode(const unsigned char *data, int len) {
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *mem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, mem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, data, len);
    BIO_flush(b64);
    BUF_MEM *bptr;
    BIO_get_mem_ptr(b64, &bptr);
    char *result = malloc(bptr->length + 1);
    memcpy(result, bptr->data, bptr->length);
    result[bptr->length] = '\0';
    BIO_free_all(b64);
    return result;
}

static int cmp_kv(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

char *build_signed_url(const char *action,
                       const char **extra_keys, const char **extra_vals, int extra_count) {
    time_t now = time(NULL);
    struct tm *gm = gmtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gm);

    char *nonce = g_uuid_string_random();

    int total = BASE_PARAM_COUNT + 1 + extra_count;
    const char *all_keys[BASE_PARAM_COUNT + 1 + MAX_EXTRA_PARAMS];
    char *all_vals[BASE_PARAM_COUNT + 1 + MAX_EXTRA_PARAMS];

    all_keys[0] = "AccessKeyId";     all_vals[0] = url_encode(get_access_key_id());
    all_keys[1] = "Action";          all_vals[1] = url_encode(action);
    all_keys[2] = "Format";          all_vals[2] = url_encode("JSON");
    all_keys[3] = "SignatureMethod"; all_vals[3] = url_encode("HMAC-SHA1");
    all_keys[4] = "SignatureNonce";  all_vals[4] = url_encode(nonce);
    all_keys[5] = "SignatureVersion";all_vals[5] = url_encode("1.0");
    all_keys[6] = "Timestamp";       all_vals[6] = url_encode(timestamp);
    all_keys[7] = "Version";         all_vals[7] = url_encode(API_VERSION);

    for (int i = 0; i < extra_count && i < MAX_EXTRA_PARAMS; i++) {
        all_keys[BASE_PARAM_COUNT + 1 + i] = extra_keys[i];
        all_vals[BASE_PARAM_COUNT + 1 + i] = url_encode(extra_vals[i]);
    }

    char *pairs[BASE_PARAM_COUNT + 1 + MAX_EXTRA_PARAMS];
    for (int i = 0; i < total; i++) {
        char *ek = url_encode(all_keys[i]);
        size_t plen = strlen(ek) + 1 + strlen(all_vals[i]) + 1;
        pairs[i] = malloc(plen);
        snprintf(pairs[i], plen, "%s=%s", ek, all_vals[i]);
        free(ek);
    }

    qsort(pairs, total, sizeof(char *), cmp_kv);

    char query[8192] = "";
    for (int i = 0; i < total; i++) {
        if (i > 0) strcat(query, "&");
        strcat(query, pairs[i]);
    }

    char *eq = url_encode(query);
    char sts[8400];
    snprintf(sts, sizeof(sts), "GET&%%2F&%s", eq);
    free(eq);

    char key_secret[256];
    const char *sk = get_access_key_secret();
    snprintf(key_secret, sizeof(key_secret), "%s&", sk);

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int dlen = 0;
    HMAC(EVP_sha1(), key_secret, strlen(key_secret),
         (unsigned char *)sts, strlen(sts), digest, &dlen);

    char *sig_b64 = base64_encode(digest, dlen);
    char *sig_enc = url_encode(sig_b64);

    char *url = malloc(16384);
    snprintf(url, 16384, "https://%s/?%s&Signature=%s", ENDPOINT, query, sig_enc);

    free(sig_enc);
    free(sig_b64);
    g_free(nonce);
    for (int i = 0; i < total; i++) {
        free(pairs[i]);
        free(all_vals[i]);
    }
    return url;
}

struct CurlBuffer {
    char *data;
    size_t size;
};

static size_t curl_write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total = size * nmemb;
    struct CurlBuffer *buf = (struct CurlBuffer *)userp;
    char *tmp = realloc(buf->data, buf->size + total + 1);
    if (!tmp) return 0;
    buf->data = tmp;
    memcpy(buf->data + buf->size, contents, total);
    buf->size += total;
    buf->data[buf->size] = '\0';
    return total;
}

static struct json_object *do_http_get(const char *url, char *errbuf, size_t errlen) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        snprintf(errbuf, errlen, "Failed to init curl");
        return NULL;
    }

    struct CurlBuffer buf = {malloc(1), 0};
    buf.data[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        snprintf(errbuf, errlen, "Network: %s", curl_easy_strerror(res));
        free(buf.data);
        return NULL;
    }

    struct json_object *root = json_tokener_parse(buf.data);
    free(buf.data);
    if (!root) {
        snprintf(errbuf, errlen, "Invalid JSON response");
        return NULL;
    }

    struct json_object *code_obj;
    if (json_object_object_get_ex(root, "Code", &code_obj)) {
        const char *code = json_object_get_string(code_obj);
        if (strcmp(code, "Success") != 0) {
            struct json_object *msg_obj;
            const char *msg = "Unknown error";
            if (json_object_object_get_ex(root, "Message", &msg_obj))
                msg = json_object_get_string(msg_obj);
            snprintf(errbuf, errlen, "%s", msg);
            json_object_put(root);
            return NULL;
        }
    }

    return root;
}

BillingData fetch_billing(void) {
    BillingData result = {0};

    time_t now = time(NULL);
    struct tm *gm = gmtime(&now);
    char cycle[16];
    strftime(cycle, sizeof(cycle), "%Y-%m", gm);

    const char *ek[] = {"BillingCycle"};
    const char *ev[] = {cycle};
    char *url = build_signed_url("QueryBillOverview", ek, ev, 1);

    struct json_object *root = do_http_get(url, result.error, sizeof(result.error));
    free(url);
    if (!root) return result;

    struct json_object *data_obj, *items_obj, *item_arr;
    if (!json_object_object_get_ex(root, "Data", &data_obj) ||
        !json_object_object_get_ex(data_obj, "Items", &items_obj) ||
        !json_object_object_get_ex(items_obj, "Item", &item_arr)) {
        json_object_put(root);
        return result;
    }

    int len = json_object_array_length(item_arr);
    result.total = 0.0;
    result.count = 0;

    for (int i = 0; i < len && result.count < MAX_ITEMS; i++) {
        struct json_object *item = json_object_array_get_idx(item_arr, i);
        struct json_object *amount_obj, *name_obj;

        if (!json_object_object_get_ex(item, "PretaxAmount", &amount_obj))
            continue;

        double amount = json_object_get_double(amount_obj);
        if (amount <= 0.001)
            continue;

        const char *name = "Unknown";
        if (json_object_object_get_ex(item, "ProductName", &name_obj))
            name = json_object_get_string(name_obj);

        strncpy(result.items[result.count].product, name,
                sizeof(result.items[result.count].product) - 1);
        result.items[result.count].amount = amount;
        result.total += amount;
        result.count++;
    }

    result.success = TRUE;
    json_object_put(root);
    return result;
}

DetailedBillingData fetch_instance_bills(void) {
    DetailedBillingData result = {0};

    time_t now = time(NULL);
    struct tm *gm = gmtime(&now);
    char cycle[16];
    strftime(cycle, sizeof(cycle), "%Y-%m", gm);

    char *next_token = NULL;
    result.total_spend = 0.0;

    do {
        const char *ek[3] = {"BillingCycle", "MaxResults"};
        const char *ev[3] = {cycle, "300"};
        int param_count = 2;

        if (next_token) {
            ek[2] = "NextToken";
            ev[2] = next_token;
            param_count = 3;
        }

        char *url = build_signed_url("DescribeInstanceBill", ek, ev, param_count);
        struct json_object *root = do_http_get(url, result.error, sizeof(result.error));
        free(url);

        if (!root) {
            free(next_token);
            return result;
        }

        struct json_object *data_obj, *items_arr;
        if (!json_object_object_get_ex(root, "Data", &data_obj) ||
            !json_object_object_get_ex(data_obj, "Items", &items_arr)) {
            json_object_put(root);
            free(next_token);
            return result;
        }

        int len = json_object_array_length(items_arr);

        for (int i = 0; i < len; i++) {
            struct json_object *item = json_object_array_get_idx(items_arr, i);
            struct json_object *product_obj, *product_code_obj, *instance_obj, *amount_obj, *region_obj;

            if (!json_object_object_get_ex(item, "ProductName", &product_obj) ||
                !json_object_object_get_ex(item, "ProductCode", &product_code_obj) ||
                !json_object_object_get_ex(item, "InstanceID", &instance_obj) ||
                !json_object_object_get_ex(item, "PretaxAmount", &amount_obj))
                continue;

            const char *product_name = json_object_get_string(product_obj);
            const char *product_code = json_object_get_string(product_code_obj);
            const char *instance_id = json_object_get_string(instance_obj);
            double amount = json_object_get_double(amount_obj);

            const char *region = "Unknown";
            if (json_object_object_get_ex(item, "Region", &region_obj)) {
                region = json_object_get_string(region_obj);
                if (strlen(region) == 0) region = "Global";
            }

            if (amount < 0.001)
                continue;

            result.total_spend += amount;

            gboolean found_service = FALSE;
            for (int j = 0; j < result.service_count; j++) {
                if (strcmp(result.services[j].name, product_name) == 0) {
                    result.services[j].amount += amount;
                    found_service = TRUE;
                    break;
                }
            }
            if (!found_service && result.service_count < MAX_ITEMS) {
                strncpy(result.services[result.service_count].name, product_name,
                        sizeof(result.services[result.service_count].name) - 1);
                result.services[result.service_count].amount = amount;
                result.service_count++;
            }

            if (strcmp(product_code, "sfm") == 0) {
                char instance_copy[512];
                strncpy(instance_copy, instance_id, sizeof(instance_copy) - 1);
                instance_copy[sizeof(instance_copy) - 1] = '\0';

                char *parts[8];
                int part_count = 0;
                char *token = strtok(instance_copy, ";");
                while (token && part_count < 8) {
                    parts[part_count++] = token;
                    token = strtok(NULL, ";");
                }

                if (part_count >= 3) {
                    const char *model_name = parts[2];

                    gboolean found_model = FALSE;
                    for (int j = 0; j < result.model_count; j++) {
                        if (strcmp(result.models[j].model, model_name) == 0 &&
                            strcmp(result.models[j].region, region) == 0) {
                            result.models[j].amount += amount;
                            found_model = TRUE;
                            break;
                        }
                    }
                    if (!found_model && result.model_count < MAX_ITEMS) {
                        strncpy(result.models[result.model_count].model, model_name,
                                sizeof(result.models[result.model_count].model) - 1);
                        strncpy(result.models[result.model_count].region, region,
                                sizeof(result.models[result.model_count].region) - 1);
                        result.models[result.model_count].amount = amount;
                        result.model_count++;
                    }
                }
            }
        }

        struct json_object *next_token_obj;
        if (json_object_object_get_ex(data_obj, "NextToken", &next_token_obj)) {
            const char *nt = json_object_get_string(next_token_obj);
            if (nt && strlen(nt) > 0) {
                free(next_token);
                next_token = strdup(nt);
            } else {
                free(next_token);
                next_token = NULL;
            }
        } else {
            free(next_token);
            next_token = NULL;
        }

        json_object_put(root);
    } while (next_token);

    result.success = TRUE;
    return result;
}

HistoricalBillingData fetch_historical_bills(void) {
    HistoricalBillingData result = {0};

    time_t now = time(NULL);
    struct tm *gm = gmtime(&now);

    for (int month_offset = 0; month_offset < 6 && result.count < MAX_ITEMS * 6; month_offset++) {
        char cycle[16];
        struct tm month_tm = *gm;
        month_tm.tm_mon -= month_offset;
        mktime(&month_tm);
        strftime(cycle, sizeof(cycle), "%Y-%m", &month_tm);

        const char *ek[] = {"BillingCycle"};
        const char *ev[] = {cycle};
        char *url = build_signed_url("QueryBillOverview", ek, ev, 1);

        char errbuf[256];
        struct json_object *root = do_http_get(url, errbuf, sizeof(errbuf));
        free(url);
        if (!root) continue;

        struct json_object *data_obj, *items_obj, *item_arr;
        if (!json_object_object_get_ex(root, "Data", &data_obj) ||
            !json_object_object_get_ex(data_obj, "Items", &items_obj) ||
            !json_object_object_get_ex(items_obj, "Item", &item_arr)) {
            json_object_put(root);
            continue;
        }

        int len = json_object_array_length(item_arr);
        for (int i = 0; i < len && result.count < MAX_ITEMS * 6; i++) {
            struct json_object *item = json_object_array_get_idx(item_arr, i);
            struct json_object *name_obj, *amount_obj;

            if (!json_object_object_get_ex(item, "ProductName", &name_obj) ||
                !json_object_object_get_ex(item, "PretaxAmount", &amount_obj))
                continue;

            double amount = json_object_get_double(amount_obj);
            if (amount < 0.01) continue;

            strncpy(result.entries[result.count].service, json_object_get_string(name_obj),
                    sizeof(result.entries[result.count].service) - 1);
            strncpy(result.entries[result.count].month, cycle,
                    sizeof(result.entries[result.count].month) - 1);
            result.entries[result.count].amount = amount;
            result.count++;
        }

        json_object_put(root);
    }

    result.success = TRUE;
    return result;
}

ModelDetailData fetch_model_detail(const char *model_name, const char *region) {
    ModelDetailData result = {0};
    strncpy(result.model, model_name, sizeof(result.model) - 1);
    strncpy(result.region, region, sizeof(result.region) - 1);

    time_t now = time(NULL);
    struct tm *gm = gmtime(&now);
    char cycle[16];
    strftime(cycle, sizeof(cycle), "%Y-%m", gm);

    result.day_count = 0;
    result.total_cost = 0.0;
    result.total_tokens = 0.0;
    result.total_input_tokens = 0;
    result.total_output_tokens = 0;

    char *next_token = NULL;
    int page = 0;
    const int max_pages = 10;

    do {
        const char *ek[3] = {"BillingCycle", "ProductCode", "MaxResults"};
        const char *ev[3] = {cycle, "sfm", "300"};
        int ek_count = 3;

        char *url = NULL;
        if (next_token) {
            const char *ek2[4] = {"BillingCycle", "ProductCode", "MaxResults", "NextToken"};
            const char *ev2[4] = {cycle, "sfm", "300", next_token};
            url = build_signed_url("DescribeInstanceBill", ek2, ev2, 4);
        } else {
            url = build_signed_url("DescribeInstanceBill", ek, ev, ek_count);
        }

        struct json_object *root = do_http_get(url, result.error, sizeof(result.error));
        free(url);

        if (!root) {
            free(next_token);
            result.success = FALSE;
            return result;
        }

        struct json_object *data_obj, *items_arr;
        if (!json_object_object_get_ex(root, "Data", &data_obj) ||
            !json_object_object_get_ex(data_obj, "Items", &items_arr)) {
            json_object_put(root);
            free(next_token);
            result.success = FALSE;
            snprintf(result.error, sizeof(result.error), "Invalid response structure");
            return result;
        }

        int len = json_object_array_length(items_arr);

        for (int i = 0; i < len; i++) {
            struct json_object *item = json_object_array_get_idx(items_arr, i);
            struct json_object *instance_obj, *amount_obj, *region_obj;

            if (!json_object_object_get_ex(item, "InstanceID", &instance_obj) ||
                !json_object_object_get_ex(item, "PretaxAmount", &amount_obj))
                continue;

            const char *instance_id = json_object_get_string(instance_obj);
            double amount = json_object_get_double(amount_obj);

            const char *item_region = "";
            if (json_object_object_get_ex(item, "Region", &region_obj)) {
                item_region = json_object_get_string(region_obj);
            }

            if (strstr(item_region, region) == NULL) continue;

            char instance_copy[512];
            strncpy(instance_copy, instance_id, sizeof(instance_copy) - 1);
            instance_copy[sizeof(instance_copy) - 1] = '\0';

            char *parts[8];
            int part_count = 0;
            char *token = strtok(instance_copy, ";");
            while (token && part_count < 8) {
                parts[part_count++] = token;
                token = strtok(NULL, ";");
            }

            if (part_count >= 3 && strcmp(parts[2], model_name) == 0) {
                // Parse token counts from InstanceID or Usage field
                long input_tokens = 0, output_tokens = 0;
                
                // Try to extract from parts[4] and parts[5] if they exist (format: ...;input;output)
                if (part_count >= 6) {
                    input_tokens = strtol(parts[4], NULL, 10);
                    output_tokens = strtol(parts[5], NULL, 10);
                }
                
                // Also check for Usage field in JSON
                struct json_object *usage_obj;
                if (json_object_object_get_ex(item, "Usage", &usage_obj)) {
                    const char *usage_str = json_object_get_string(usage_obj);
                    // Parse usage string if it contains token info
                    // Format might be "input:123,output:456" or similar
                    if (usage_str && strstr(usage_str, "input")) {
                        const char *in_pos = strstr(usage_str, "input:");
                        if (in_pos) {
                            input_tokens = strtol(in_pos + 6, NULL, 10);
                        }
                    }
                    if (usage_str && strstr(usage_str, "output")) {
                        const char *out_pos = strstr(usage_str, "output:");
                        if (out_pos) {
                            output_tokens = strtol(out_pos + 7, NULL, 10);
                        }
                    }
                }
                
                if (result.day_count < 31) {
                    const char *billing_type = (part_count >= 4) ? parts[3] : "unknown";
                    strncpy(result.daily[result.day_count].date, billing_type,
                            sizeof(result.daily[result.day_count].date) - 1);
                    result.daily[result.day_count].cost = amount;
                    result.daily[result.day_count].input_tokens = input_tokens;
                    result.daily[result.day_count].output_tokens = output_tokens;
                    result.daily[result.day_count].tokens = input_tokens + output_tokens;
                    result.day_count++;
                }
                result.total_cost += amount;
                result.total_input_tokens += input_tokens;
                result.total_output_tokens += output_tokens;
                result.total_tokens += input_tokens + output_tokens;
            }
        }

        struct json_object *next_token_obj;
        if (json_object_object_get_ex(data_obj, "NextToken", &next_token_obj)) {
            const char *nt = json_object_get_string(next_token_obj);
            free(next_token);
            next_token = (nt && strlen(nt) > 0) ? strdup(nt) : NULL;
        } else {
            free(next_token);
            next_token = NULL;
        }

        json_object_put(root);
        page++;
    } while (next_token && page < max_pages);

    free(next_token);
    result.success = TRUE;
    // Save to database
    if (result.success && result.day_count > 0) {
        if (db_init() == 0) {
            BillingSnapshot snapshot = {0};
            snapshot.timestamp = now;
            strncpy(snapshot.model, model_name, sizeof(snapshot.model) - 1);
            strncpy(snapshot.region, region, sizeof(snapshot.region) - 1);
            snapshot.cost = result.total_cost;
            snapshot.input_tokens = result.total_input_tokens;
            snapshot.output_tokens = result.total_output_tokens;
            db_insert_snapshot(&snapshot);
        }
    }
    
    return result;
}

/* ── background thread + idle dispatch ───────────────────────────── */

typedef struct {
    BillingCardWidgets *widgets;
    BillingData data;
} FetchResult;

static gboolean update_ui_dispatch(gpointer user_data) {
    FetchResult *fr = (FetchResult *)user_data;
    extern void update_billing_card(BillingCardWidgets *w, BillingData *bd);
    update_billing_card(fr->widgets, &fr->data);
    g_free(fr);
    return G_SOURCE_REMOVE;
}

static gpointer fetch_thread_fn(gpointer user_data) {
    BillingCardWidgets *w = (BillingCardWidgets *)user_data;
    FetchResult *fr = g_malloc(sizeof(FetchResult));
    fr->widgets = w;
    fr->data = fetch_billing();
    g_idle_add(update_ui_dispatch, fr);
    return NULL;
}

void start_fetch(BillingCardWidgets *w) {
    gtk_widget_set_visible(w->spinner, TRUE);
    gtk_spinner_start(GTK_SPINNER(w->spinner));
    g_thread_new("billing-fetch", fetch_thread_fn, w);
}

/* ── detailed background thread + idle dispatch ──────────────────── */

typedef struct {
    ServicesCardWidgets *services_w;
    ModelsCardWidgets *models_beijing_w;
    ModelsCardWidgets *models_singapore_w;
    DetailedBillingData data;
} DetailedFetchResult;

static gboolean update_detailed_ui_dispatch(gpointer user_data) {
    DetailedFetchResult *fr = (DetailedFetchResult *)user_data;
    extern void update_services_card(ServicesCardWidgets *w, DetailedBillingData *bd);
    extern void update_models_card(ModelsCardWidgets *w, DetailedBillingData *bd);
    update_services_card(fr->services_w, &fr->data);
    update_models_card(fr->models_beijing_w, &fr->data);
    update_models_card(fr->models_singapore_w, &fr->data);
    g_free(fr);
    return G_SOURCE_REMOVE;
}

typedef struct {
    ServicesCardWidgets *services_w;
    ModelsCardWidgets *models_beijing_w;
    ModelsCardWidgets *models_singapore_w;
} DetailedFetchContext;

static gpointer detailed_fetch_thread_fn(gpointer user_data) {
    DetailedFetchContext *ctx = (DetailedFetchContext *)user_data;
    DetailedFetchResult *fr = g_malloc(sizeof(DetailedFetchResult));
    fr->services_w = ctx->services_w;
    fr->models_beijing_w = ctx->models_beijing_w;
    fr->models_singapore_w = ctx->models_singapore_w;
    fr->data = fetch_instance_bills();
    g_idle_add(update_detailed_ui_dispatch, fr);
    g_free(ctx);
    return NULL;
}

void start_detailed_fetch(ServicesCardWidgets *sw, ModelsCardWidgets *mw_beijing, ModelsCardWidgets *mw_singapore) {
    gtk_widget_set_visible(sw->spinner, TRUE);
    gtk_spinner_start(GTK_SPINNER(sw->spinner));
    gtk_widget_set_visible(mw_beijing->spinner, TRUE);
    gtk_spinner_start(GTK_SPINNER(mw_beijing->spinner));
    gtk_widget_set_visible(mw_singapore->spinner, TRUE);
    gtk_spinner_start(GTK_SPINNER(mw_singapore->spinner));

    DetailedFetchContext *ctx = g_malloc(sizeof(DetailedFetchContext));
    ctx->services_w = sw;
    ctx->models_beijing_w = mw_beijing;
    ctx->models_singapore_w = mw_singapore;
    g_thread_new("detailed-billing-fetch", detailed_fetch_thread_fn, ctx);
}

typedef struct {
    HistoricalCardWidgets *history_w;
    HistoricalBillingData data;
} HistoricalFetchResult;

static gboolean update_historical_ui_dispatch(gpointer user_data) {
    HistoricalFetchResult *fr = (HistoricalFetchResult *)user_data;
    extern void update_historical_card(HistoricalCardWidgets *w, HistoricalBillingData *bd);
    update_historical_card(fr->history_w, &fr->data);
    g_free(fr);
    return G_SOURCE_REMOVE;
}

static gpointer historical_fetch_thread_fn(gpointer user_data) {
    HistoricalCardWidgets *hw = (HistoricalCardWidgets *)user_data;
    HistoricalFetchResult *fr = g_malloc(sizeof(HistoricalFetchResult));
    fr->history_w = hw;
    fr->data = fetch_historical_bills();
    g_idle_add(update_historical_ui_dispatch, fr);
    return NULL;
}

void start_historical_fetch(HistoricalCardWidgets *hw) {
    gtk_widget_set_visible(hw->spinner, TRUE);
    gtk_spinner_start(GTK_SPINNER(hw->spinner));
    g_thread_new("historical-billing-fetch", historical_fetch_thread_fn, hw);
}

