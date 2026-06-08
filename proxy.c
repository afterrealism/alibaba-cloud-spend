#ifndef _WIN32
/* Proxy server - Unix/Linux only (requires POSIX sockets, pthreads) */
#include "proxy.h"
#include <sqlite3.h>
#include <json-c/json.h>
#include <curl/curl.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#define PROXY_PORT 8888
#define PROXY_BIND "127.0.0.1"
#define UPSTREAM_URL "https://dashscope.aliyuncs.com/compatible-mode"
#define DB_PATH "/home/sheece/.omp/proxy/dashscope-usage.db"
#define LOG_PATH "/home/sheece/.omp/dashscope-proxy.log"
#define MAX_BODY_SIZE (16 * 1024 * 1024)
#define MAX_HEADER_SIZE (64 * 1024)
#define MAX_CONNECTIONS 100
#define SOCKET_TIMEOUT 30

/* ── Pricing Map (verbatim from dashscope-proxy.js:23-136) ────────────────── */

typedef struct {
    const char *model;
    double input;
    double output;
} PriceEntry;

static const PriceEntry PRICING[] = {
    // QWEN-MAX
    {"qwen3.7-max", 1.65, 4.95},
    {"qwen3.7-max-2026-05-20", 1.65, 4.95},
    {"qwen3.6-max-preview", 1.24, 7.43},
    {"qwen3-max", 0.36, 1.43},
    {"qwen3-max-2026-01-23", 0.36, 1.43},
    {"qwen3-max-2025-09-23", 0.86, 3.44},
    {"qwen3-max-preview", 0.86, 3.44},
    {"qwen-max", 0.35, 1.38},
    {"qwen-max-latest", 0.35, 1.38},
    // QWEN-PLUS
    {"qwen3.6-plus", 0.28, 1.65},
    {"qwen3.6-plus-2026-04-02", 0.28, 1.65},
    {"qwen3.5-plus", 0.12, 0.69},
    {"qwen3.5-plus-2026-04-20", 0.12, 0.69},
    {"qwen3.5-plus-2026-02-15", 0.12, 0.69},
    {"qwen-plus", 0.12, 0.29},
    {"qwen-plus-latest", 0.12, 0.29},
    {"qwen-plus-2025-12-01", 0.12, 0.29},
    // QWEN-FLASH
    {"qwen3.6-flash", 0.17, 0.99},
    {"qwen3.6-flash-2026-04-16", 0.17, 0.99},
    {"qwen3.5-flash", 0.03, 0.29},
    {"qwen3.5-flash-2026-02-23", 0.03, 0.29},
    {"qwen-flash", 0.02, 0.22},
    {"qwen-flash-2025-07-28", 0.02, 0.22},
    // QWEN-TURBO
    {"qwen-turbo", 0.04, 0.09},
    {"qwen-turbo-latest", 0.04, 0.09},
    // QWQ
    {"qwq-plus", 0.23, 0.57},
    {"qwq-plus-latest", 0.23, 0.57},
    {"qwq-32b", 0.29, 0.86},
    {"qwq-32b-preview", 0.29, 0.86},
    // QWEN-CODER
    {"qwen3-coder-next", 0.14, 0.57},
    {"qwen3-coder-480b-a35b-instruct", 0.86, 3.44},
    {"qwen3-coder-30b-a3b-instruct", 0.22, 0.86},
    {"qwen3-coder-plus", 0.50, 2.00},
    {"qwen3-coder-plus-2025-07-22", 0.50, 2.00},
    {"qwen3-coder-flash", 0.10, 0.40},
    {"qwen3-coder-flash-2025-07-28", 0.10, 0.40},
    {"qwen2.5-coder-32b-instruct", 0.29, 0.86},
    {"qwen2.5-coder-14b-instruct", 0.29, 0.86},
    {"qwen2.5-coder-7b-instruct", 0.14, 0.29},
    // QWEN-MATH
    {"qwen-math-plus", 0.57, 1.72},
    {"qwen-math-turbo", 0.29, 0.86},
    // QWEN3.6 OPEN-SOURCE
    {"qwen3.6-35b-a3b", 0.25, 1.49},
    {"qwen3.6-27b", 0.41, 2.48},
    // QWEN3.5 OPEN-SOURCE
    {"qwen3.5-397b-a17b", 0.17, 1.03},
    {"qwen3.5-122b-a10b", 0.12, 0.92},
    {"qwen3.5-27b", 0.09, 0.69},
    {"qwen3.5-35b-a3b", 0.06, 0.46},
    // QWEN3 OPEN-SOURCE
    {"qwen3-next-80b-a3b-thinking", 0.14, 1.43},
    {"qwen3-next-80b-a3b-instruct", 0.14, 0.57},
    {"qwen3-235b-a22b-thinking-2507", 0.29, 2.87},
    {"qwen3-235b-a22b-instruct-2507", 0.29, 1.15},
    {"qwen3-30b-a3b-thinking-2507", 0.11, 1.08},
    {"qwen3-30b-a3b-instruct-2507", 0.11, 0.43},
    {"qwen3-235b-a22b", 0.29, 1.15},
    {"qwen3-32b", 0.29, 1.15},
    {"qwen3-30b-a3b", 0.11, 0.43},
    {"qwen3-14b", 0.14, 0.57},
    {"qwen3-8b", 0.07, 0.29},
    {"qwen3-4b", 0.04, 0.17},
    // QWEN2.5
    {"qwen2.5-14b-instruct-1m", 0.14, 0.43},
    {"qwen2.5-7b-instruct-1m", 0.07, 0.14},
    {"qwen2.5-72b-instruct", 0.57, 1.72},
    {"qwen2.5-32b-instruct", 0.29, 0.86},
    {"qwen2.5-14b-instruct", 0.14, 0.43},
    {"qwen2.5-7b-instruct", 0.07, 0.14},
    // QWEN-LONG
    {"qwen-long-latest", 0.07, 0.29},
    // QVQ
    {"qvq-max", 1.15, 4.59},
    {"qvq-plus", 0.29, 0.72},
    {"qvq-72b-preview", 1.72, 5.16},
    // QWEN2.5 MATH
    {"qwen2.5-math-72b-instruct", 0.57, 1.72},
    {"qwen2.5-math-7b-instruct", 0.14, 0.29},
    // QWEN-MT
    {"qwen-mt-plus", 0.26, 0.78},
    {"qwen-mt-flash", 0.10, 0.28},
    // DEEPSEEK
    {"deepseek-v4-pro", 1.65, 3.30},
    {"deepseek-v4-flash", 0.14, 0.28},
    {"deepseek-v3.2", 0.29, 0.43},
    {"deepseek-v3.2-exp", 0.29, 0.43},
    {"deepseek-v3.1", 0.57, 1.72},
    {"deepseek-r1", 0.57, 2.29},
    {"deepseek-r1-0528", 0.57, 2.29},
    {"deepseek-v3", 0.29, 1.15},
    {"deepseek-r1-distill-qwen-32b", 0.29, 0.86},
    {"deepseek-r1-distill-qwen-14b", 0.14, 0.43},
    {"deepseek-r1-distill-qwen-7b", 0.07, 0.14},
    // KIMI
    {"kimi-k2.6", 0.89, 3.71},
    {"kimi-k2.5", 0.57, 3.01},
    {"kimi-k2-thinking", 0.57, 2.29},
    {"Moonshot-Kimi-K2-Instruct", 0.57, 2.29},
    // MINIMAX
    {"MiniMax-M2.5", 0.30, 1.21},
    // GLM
    {"glm-5.1", 0.83, 3.30},
    {"glm-5", 0.57, 2.58},
    {"glm-4.7", 0.43, 2.01},
    {"glm-4.6", 0.43, 2.01},
};

static const int PRICING_COUNT = sizeof(PRICING) / sizeof(PRICING[0]);

/* ── Globals ──────────────────────────────────────────────────────────────── */

static sqlite3 *g_db = NULL;
static pthread_mutex_t g_db_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_listen_fd = -1;
static pthread_t g_listener_thread;
static volatile int g_running = 0;
static char g_api_key[512] = {0};

static volatile long g_total_requests = 0;
static volatile long g_active_connections = 0;
static volatile long g_error_count = 0;
static char g_last_model[128] = {0};
static char g_last_status[32] = {0};
static double g_last_cost_usd = 0.0;
static time_t g_started_at = 0;

/* ── Logging ──────────────────────────────────────────────────────────────── */

static void log_proxy(const char *fmt, ...) {
    FILE *f = fopen(LOG_PATH, "a");
    if (!f) return;
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", tm_info);
    
    fprintf(f, "[%s] ", timestamp);
    
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    
    fprintf(f, "\n");
    fclose(f);
}

/* ── Database Setup (verbatim from dashscope-proxy.js:157-224) ────────────── */

static int db_setup(void) {
    const char *schema = 
        "CREATE TABLE IF NOT EXISTS requests ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp TEXT NOT NULL DEFAULT (datetime('now')),"
        "  model TEXT NOT NULL,"
        "  prompt_tokens INTEGER NOT NULL DEFAULT 0,"
        "  completion_tokens INTEGER NOT NULL DEFAULT 0,"
        "  total_tokens INTEGER NOT NULL DEFAULT 0,"
        "  cached_tokens INTEGER NOT NULL DEFAULT 0,"
        "  cache_creation_tokens INTEGER NOT NULL DEFAULT 0,"
        "  uncached_input_tokens INTEGER NOT NULL DEFAULT 0,"
        "  output_tokens INTEGER NOT NULL DEFAULT 0,"
        "  input_cost REAL NOT NULL DEFAULT 0,"
        "  output_cost REAL NOT NULL DEFAULT 0,"
        "  cache_read_cost REAL NOT NULL DEFAULT 0,"
        "  cache_write_cost REAL NOT NULL DEFAULT 0,"
        "  total_cost REAL NOT NULL DEFAULT 0,"
        "  stream INTEGER NOT NULL DEFAULT 0,"
        "  temperature REAL,"
        "  max_tokens INTEGER,"
        "  latency_ms INTEGER,"
        "  status_code INTEGER,"
        "  error TEXT,"
        "  request_messages TEXT,"
        "  response_content TEXT"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_requests_timestamp ON requests(timestamp);"
        "CREATE INDEX IF NOT EXISTS idx_requests_model ON requests(model);";
    
    char *err_msg = NULL;
    if (sqlite3_exec(g_db, schema, NULL, NULL, &err_msg) != SQLITE_OK) {
        log_proxy("DB SCHEMA ERROR: %s", err_msg);
        sqlite3_free(err_msg);
        return -1;
    }
    
    const char *views = 
        "CREATE VIEW IF NOT EXISTS v_daily_summary AS "
        "SELECT date(timestamp) as day, model, "
        "  SUM(prompt_tokens) as input_tokens, "
        "  SUM(completion_tokens) as output_tokens, "
        "  SUM(cached_tokens) as cached_tokens, "
        "  SUM(cache_creation_tokens) as cache_creation_tokens, "
        "  SUM(total_cost) as total_cost, "
        "  COUNT(*) as request_count "
        "FROM requests GROUP BY day, model "
        "ORDER BY day DESC, model;"
        
        "CREATE VIEW IF NOT EXISTS v_monthly_summary AS "
        "SELECT strftime('%Y-%m', timestamp) as month, model, "
        "  SUM(prompt_tokens) as input_tokens, "
        "  SUM(completion_tokens) as output_tokens, "
        "  SUM(cached_tokens) as cached_tokens, "
        "  SUM(cache_creation_tokens) as cache_creation_tokens, "
        "  SUM(total_cost) as total_cost, "
        "  COUNT(*) as request_count "
        "FROM requests GROUP BY month, model "
        "ORDER BY month DESC, model;"
        
        "CREATE VIEW IF NOT EXISTS v_total_summary AS "
        "SELECT model, "
        "  SUM(prompt_tokens) as input_tokens, "
        "  SUM(completion_tokens) as output_tokens, "
        "  SUM(cached_tokens) as cached_tokens, "
        "  SUM(cache_creation_tokens) as cache_creation_tokens, "
        "  SUM(total_cost) as total_cost, "
        "  COUNT(*) as request_count "
        "FROM requests GROUP BY model "
        "ORDER BY total_cost DESC;";
    
    if (sqlite3_exec(g_db, views, NULL, NULL, &err_msg) != SQLITE_OK) {
        log_proxy("DB VIEWS ERROR: %s", err_msg);
        sqlite3_free(err_msg);
        return -1;
    }
    
    return 0;
}

/* ── Pricing Lookup ───────────────────────────────────────────────────────── */

typedef struct {
    double input;
    double output;
    double cache_read;
    double cache_write;
    int found;
} Price;

static Price get_price(const char *model) {
    Price p = {0};
    for (int i = 0; i < PRICING_COUNT; i++) {
        if (strcmp(PRICING[i].model, model) == 0) {
            p.input = PRICING[i].input;
            p.output = PRICING[i].output;
            p.cache_read = PRICING[i].input * 0.10;
            p.cache_write = PRICING[i].input * 1.25;
            p.found = 1;
            return p;
        }
    }
    return p;
}

/* ── Cost Computation (port of dashscope-proxy.js:352-410) ────────────────── */

typedef struct {
    long prompt_tokens;
    long completion_tokens;
    long total_tokens;
    long cached_tokens;
    long cache_creation_tokens;
    long uncached_input_tokens;
    long output_tokens;
    double input_cost;
    double output_cost;
    double cache_read_cost;
    double cache_write_cost;
    double total_cost;
} CostResult;

static CostResult compute_cost(const char *model, json_object *usage) {
    CostResult r = {0};
    
    if (!usage) return r;
    
    json_object *obj;
    if (json_object_object_get_ex(usage, "prompt_tokens", &obj))
        r.prompt_tokens = json_object_get_int64(obj);
    if (json_object_object_get_ex(usage, "completion_tokens", &obj))
        r.completion_tokens = json_object_get_int64(obj);
    if (json_object_object_get_ex(usage, "total_tokens", &obj))
        r.total_tokens = json_object_get_int64(obj);
    else
        r.total_tokens = r.prompt_tokens + r.completion_tokens;
    
    // Check prompt_tokens_details
    json_object *details;
    if (json_object_object_get_ex(usage, "prompt_tokens_details", &details)) {
        if (json_object_object_get_ex(details, "cached_tokens", &obj))
            r.cached_tokens = json_object_get_int64(obj);
        if (json_object_object_get_ex(details, "cache_creation_input_tokens", &obj))
            r.cache_creation_tokens = json_object_get_int64(obj);
        
        // Check nested cache_creation
        json_object *cache_creation;
        if (json_object_object_get_ex(details, "cache_creation", &cache_creation)) {
            json_object *ephemeral;
            if (json_object_object_get_ex(cache_creation, "ephemeral_5m_input_tokens", &ephemeral))
                r.cache_creation_tokens = json_object_get_int64(ephemeral);
        }
    }
    
    // Check top-level fields
    if (json_object_object_get_ex(usage, "cached_tokens", &obj))
        r.cached_tokens = json_object_get_int64(obj);
    if (json_object_object_get_ex(usage, "cache_creation_tokens", &obj))
        r.cache_creation_tokens = json_object_get_int64(obj);
    
    r.uncached_input_tokens = r.prompt_tokens - r.cached_tokens - r.cache_creation_tokens;
    if (r.uncached_input_tokens < 0) r.uncached_input_tokens = 0;
    r.output_tokens = r.completion_tokens;
    
    Price price = get_price(model);
    if (!price.found) return r;
    
    const double M = 1000000.0;
    r.input_cost = (r.uncached_input_tokens / M) * price.input;
    r.output_cost = (r.output_tokens / M) * price.output;
    r.cache_read_cost = (r.cached_tokens / M) * price.cache_read;
    r.cache_write_cost = (r.cache_creation_tokens / M) * price.cache_write;
    r.total_cost = r.input_cost + r.output_cost + r.cache_read_cost + r.cache_write_cost;
    
    return r;
}

/* ── Cache Control Injection (port of dashscope-proxy.js:305-349) ─────────── */

static void inject_cache_control(json_object *messages) {
    if (!messages || !json_object_is_type(messages, json_type_array))
        return;
    
    int len = json_object_array_length(messages);
    if (len == 0) return;
    
    int target_idx = -1;
    
    // Find last system message
    for (int i = len - 1; i >= 0; i--) {
        json_object *msg = json_object_array_get_idx(messages, i);
        json_object *role;
        if (json_object_object_get_ex(msg, "role", &role)) {
            if (strcmp(json_object_get_string(role), "system") == 0) {
                target_idx = i;
                break;
            }
        }
    }
    
    // Fallback: first user message
    if (target_idx == -1) {
        for (int i = 0; i < len; i++) {
            json_object *msg = json_object_array_get_idx(messages, i);
            json_object *role;
            if (json_object_object_get_ex(msg, "role", &role)) {
                if (strcmp(json_object_get_string(role), "user") == 0) {
                    target_idx = i;
                    break;
                }
            }
        }
    }
    
    if (target_idx == -1) return;
    
    json_object *msg = json_object_array_get_idx(messages, target_idx);
    json_object *content;
    if (!json_object_object_get_ex(msg, "content", &content))
        return;
    
    if (json_object_is_type(content, json_type_string)) {
        const char *text = json_object_get_string(content);
        json_object *new_content = json_object_new_array();
        
        json_object *block = json_object_new_object();
        json_object_object_add(block, "type", json_object_new_string("text"));
        json_object_object_add(block, "text", json_object_new_string(text));
        
        json_object *cache_control = json_object_new_object();
        json_object_object_add(cache_control, "type", json_object_new_string("ephemeral"));
        json_object_object_add(block, "cache_control", cache_control);
        
        json_object_array_add(new_content, block);
        json_object_object_del(msg, "content");
        json_object_object_add(msg, "content", new_content);
    } else if (json_object_is_type(content, json_type_array)) {
        int content_len = json_object_array_length(content);
        if (content_len == 0) return;
        
        // Check if already has cache_control
        for (int i = 0; i < content_len; i++) {
            json_object *block = json_object_array_get_idx(content, i);
            json_object *cc;
            if (json_object_object_get_ex(block, "cache_control", &cc))
                return; // already injected
        }
        
        // Add to last block
        json_object *last = json_object_array_get_idx(content, content_len - 1);
        json_object *cache_control = json_object_new_object();
        json_object_object_add(cache_control, "type", json_object_new_string("ephemeral"));
        json_object_object_add(last, "cache_control", cache_control);
    }
}

/* ── Database Insert (port of dashscope-proxy.js:226-240) ─────────────────── */

static void insert_request(
    const char *model,
    long prompt_tokens, long completion_tokens, long total_tokens,
    long cached_tokens, long cache_creation_tokens, long uncached_input_tokens, long output_tokens,
    double input_cost, double output_cost, double cache_read_cost, double cache_write_cost, double total_cost,
    int stream, double temperature, int max_tokens, int latency_ms, int status_code,
    const char *error, const char *request_messages, const char *response_content
) {
    pthread_mutex_lock(&g_db_mutex);
    
    sqlite3_stmt *stmt;
    const char *sql = 
        "INSERT INTO requests ("
        "  model, prompt_tokens, completion_tokens, total_tokens,"
        "  cached_tokens, cache_creation_tokens, uncached_input_tokens, output_tokens,"
        "  input_cost, output_cost, cache_read_cost, cache_write_cost, total_cost,"
        "  stream, temperature, max_tokens, latency_ms, status_code, error,"
        "  request_messages, response_content"
        ") VALUES ("
        "  ?, ?, ?, ?,"
        "  ?, ?, ?, ?,"
        "  ?, ?, ?, ?, ?,"
        "  ?, ?, ?, ?, ?, ?,"
        "  ?, ?"
        ")";
    
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        log_proxy("DB PREPARE ERROR: %s", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&g_db_mutex);
        return;
    }
    
    sqlite3_bind_text(stmt, 1, model, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, prompt_tokens);
    sqlite3_bind_int64(stmt, 3, completion_tokens);
    sqlite3_bind_int64(stmt, 4, total_tokens);
    sqlite3_bind_int64(stmt, 5, cached_tokens);
    sqlite3_bind_int64(stmt, 6, cache_creation_tokens);
    sqlite3_bind_int64(stmt, 7, uncached_input_tokens);
    sqlite3_bind_int64(stmt, 8, output_tokens);
    sqlite3_bind_double(stmt, 9, input_cost);
    sqlite3_bind_double(stmt, 10, output_cost);
    sqlite3_bind_double(stmt, 11, cache_read_cost);
    sqlite3_bind_double(stmt, 12, cache_write_cost);
    sqlite3_bind_double(stmt, 13, total_cost);
    sqlite3_bind_int(stmt, 14, stream);
    if (temperature >= 0) sqlite3_bind_double(stmt, 15, temperature);
    else sqlite3_bind_null(stmt, 15);
    if (max_tokens > 0) sqlite3_bind_int(stmt, 16, max_tokens);
    else sqlite3_bind_null(stmt, 16);
    sqlite3_bind_int(stmt, 17, latency_ms);
    sqlite3_bind_int(stmt, 18, status_code);
    if (error) sqlite3_bind_text(stmt, 19, error, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 19);
    if (request_messages) sqlite3_bind_text(stmt, 20, request_messages, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 20);
    if (response_content) sqlite3_bind_text(stmt, 21, response_content, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 21);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        log_proxy("DB INSERT ERROR: %s", sqlite3_errmsg(g_db));
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_mutex);
}

/* ── HTTP Helpers ─────────────────────────────────────────────────────────── */

static int send_response(int client_fd, int status, const char *content_type, const char *body) {
    char header[512];
    int body_len = body ? strlen(body) : 0;
    
    const char *status_text = "OK";
    if (status == 400) status_text = "Bad Request";
    else if (status == 404) status_text = "Not Found";
    else if (status == 502) status_text = "Bad Gateway";
    
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, status_text, content_type, body_len);
    
    send(client_fd, header, header_len, MSG_NOSIGNAL);
    if (body && body_len > 0)
        send(client_fd, body, body_len, MSG_NOSIGNAL);
    
    return 0;
}

static int send_json(int client_fd, int status, json_object *json) {
    const char *body = json_object_to_json_string(json);
    int ret = send_response(client_fd, status, "application/json", body);
    json_object_put(json);
    return ret;
}

static int send_stream_header(int client_fd, int status) {
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",
        status);
    
    send(client_fd, header, header_len, MSG_NOSIGNAL);
    return 0;
}

/* ── Read HTTP Request ────────────────────────────────────────────────────── */

typedef struct {
    char method[16];
    char path[2048];
    char headers[MAX_HEADER_SIZE];
    char *body;
    int body_len;
    int content_length;
} HttpRequest;

static int read_http_request(int client_fd, HttpRequest *req) {
    memset(req, 0, sizeof(HttpRequest));
    
    // Read request line and headers
    char buffer[MAX_HEADER_SIZE];
    int total_read = 0;
    int header_end = -1;
    
    while (total_read < MAX_HEADER_SIZE - 1) {
        int n = recv(client_fd, buffer + total_read, MAX_HEADER_SIZE - 1 - total_read, 0);
        if (n <= 0) return -1;
        total_read += n;
        buffer[total_read] = '\0';
        
        char *end = strstr(buffer, "\r\n\r\n");
        if (end) {
            header_end = end - buffer + 4;
            break;
        }
    }
    
    if (header_end == -1) return -1;
    
    // Parse request line
    char *line_end = strstr(buffer, "\r\n");
    if (!line_end) return -1;
    
    sscanf(buffer, "%15s %2047s", req->method, req->path);
    
    // Copy headers
    int header_len = header_end - 4;
    if (header_len >= MAX_HEADER_SIZE) header_len = MAX_HEADER_SIZE - 1;
    memcpy(req->headers, buffer, header_len);
    req->headers[header_len] = '\0';
    
    // Find Content-Length
    char *cl = strstr(req->headers, "Content-Length:");
    if (!cl) cl = strstr(req->headers, "content-length:");
    if (cl) {
        sscanf(cl, "%*[^:]: %d", &req->content_length);
    }
    
    // Read body if present
    if (req->content_length > 0) {
        if (req->content_length > MAX_BODY_SIZE) return -1;
        
        req->body = malloc(req->content_length + 1);
        if (!req->body) return -1;
        
        int body_offset = total_read - header_end;
        if (body_offset > 0) {
            memcpy(req->body, buffer + header_end, body_offset);
        }
        
        int remaining = req->content_length - body_offset;
        int body_read = body_offset;
        
        while (remaining > 0) {
            int n = recv(client_fd, req->body + body_read, remaining, 0);
            if (n <= 0) {
                free(req->body);
                req->body = NULL;
                return -1;
            }
            body_read += n;
            remaining -= n;
        }
        
        req->body[req->content_length] = '\0';
        req->body_len = req->content_length;
    }
    
    return 0;
}

/* ── Curl Write Callback ──────────────────────────────────────────────────── */

typedef struct {
    int client_fd;
    char *buffer;
    size_t buffer_len;
    size_t buffer_cap;
} CurlContext;

static size_t proxy_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    CurlContext *ctx = (CurlContext *)userdata;
    size_t total = size * nmemb;
    
    if (ctx->client_fd >= 0) {
        // Stream mode: send directly to client
        send(ctx->client_fd, ptr, total, MSG_NOSIGNAL);
    } else {
        // Buffer mode: accumulate
        if (ctx->buffer_len + total > ctx->buffer_cap) {
            ctx->buffer_cap = ctx->buffer_len + total + 4096;
            ctx->buffer = realloc(ctx->buffer, ctx->buffer_cap);
            if (!ctx->buffer) return 0;
        }
        memcpy(ctx->buffer + ctx->buffer_len, ptr, total);
        ctx->buffer_len += total;
        ctx->buffer[ctx->buffer_len] = '\0';
    }
    
    return total;
}

/* ── Handle Chat Completions (port of dashscope-proxy.js:491-727) ─────────── */

static void handle_chat_completions(int client_fd, const char *body, int body_len) {
    struct timeval start_time;
    gettimeofday(&start_time, NULL);
    
    json_object *json_body = json_tokener_parse(body);
    if (!json_body) {
        send_response(client_fd, 400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    json_object *obj;
    const char *model = "unknown";
    if (json_object_object_get_ex(json_body, "model", &obj))
        model = json_object_get_string(obj);
    
    int is_stream = 0;
    if (json_object_object_get_ex(json_body, "stream", &obj))
        is_stream = json_object_get_boolean(obj);
    
    // DashScope rejects object/required tool_choice in thinking mode
    json_object *enable_thinking, *tool_choice;
    if (json_object_object_get_ex(json_body, "enable_thinking", &enable_thinking) &&
        json_object_get_boolean(enable_thinking) &&
        json_object_object_get_ex(json_body, "tool_choice", &tool_choice) &&
        json_object_is_type(tool_choice, json_type_object)) {
        json_object_object_del(json_body, "tool_choice");
        json_object_object_add(json_body, "tool_choice", json_object_new_string("auto"));
    }
    
    // Inject cache_control
    json_object *messages;
    if (json_object_object_get_ex(json_body, "messages", &messages))
        inject_cache_control(messages);
    
    // For streaming, ensure we get usage data
    if (is_stream) {
        json_object *stream_options = json_object_new_object();
        json_object_object_add(stream_options, "include_usage", json_object_new_boolean(1));
        json_object_object_add(json_body, "stream_options", stream_options);
    }
    
    // Get request messages for logging
    const char *request_messages_json = NULL;
    if (json_object_object_get_ex(json_body, "messages", &messages))
        request_messages_json = json_object_to_json_string(messages);
    
    double temperature = -1;
    if (json_object_object_get_ex(json_body, "temperature", &obj))
        temperature = json_object_get_double(obj);
    
    int max_tokens = 0;
    if (json_object_object_get_ex(json_body, "max_tokens", &obj))
        max_tokens = json_object_get_int(obj);
    
    // Forward to upstream
    const char *modified_body = json_object_to_json_string(json_body);
    
    char upstream_url[512];
    snprintf(upstream_url, sizeof(upstream_url), "%s/v1/chat/completions", UPSTREAM_URL);
    
    CURL *curl = curl_easy_init();
    if (!curl) {
        json_object_put(json_body);
        send_response(client_fd, 502, "application/json", "{\"error\":\"Curl init failed\"}");
        return;
    }
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    char auth_header[768];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_api_key);
    headers = curl_slist_append(headers, auth_header);
    
    CurlContext ctx = {0};
    ctx.client_fd = is_stream ? client_fd : -1;
    
    curl_easy_setopt(curl, CURLOPT_URL, upstream_url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, modified_body);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, proxy_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        struct timeval end_time;
        gettimeofday(&end_time, NULL);
        int latency_ms = (end_time.tv_sec - start_time.tv_sec) * 1000 +
                         (end_time.tv_usec - start_time.tv_usec) / 1000;
        
        log_proxy("FETCH ERROR model=%s latency=%dms err=%s", model, latency_ms, curl_easy_strerror(res));
        
        insert_request(model, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                      is_stream, temperature, max_tokens, latency_ms, 0,
                      curl_easy_strerror(res), request_messages_json, NULL);
        
        if (!is_stream) {
            char error_json[512];
            snprintf(error_json, sizeof(error_json),
                    "{\"error\":\"Upstream fetch failed: %s\"}", curl_easy_strerror(res));
            send_response(client_fd, 502, "application/json", error_json);
        }
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        json_object_put(json_body);
        return;
    }
    
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    int latency_ms = (end_time.tv_sec - start_time.tv_sec) * 1000 +
                     (end_time.tv_usec - start_time.tv_usec) / 1000;
    
    if (!is_stream) {
        // Non-streaming: parse response and compute cost
        json_object *resp_json = json_tokener_parse(ctx.buffer);
        if (!resp_json) {
            log_proxy("PARSE ERROR model=%s status=%ld latency=%dms", model, http_code, latency_ms);
            send_response(client_fd, http_code, "application/json", ctx.buffer);
            free(ctx.buffer);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            json_object_put(json_body);
            return;
        }
        
        json_object *usage;
        CostResult cost = {0};
        if (json_object_object_get_ex(resp_json, "usage", &usage))
            cost = compute_cost(model, usage);
        
        // Extract response content
        const char *response_content = NULL;
        json_object *choices;
        if (json_object_object_get_ex(resp_json, "choices", &choices)) {
            response_content = json_object_to_json_string(choices);
        }
        
        double cached_pct = cost.prompt_tokens > 0 ?
            (cost.cached_tokens * 100.0 / cost.prompt_tokens) : 0.0;
        
        log_proxy("model=%s prompt=%ld comp=%ld cached=%ld(%.1f%%) created=%ld cost=$%.6f latency=%dms status=%ld",
                 model, cost.prompt_tokens, cost.completion_tokens, cost.cached_tokens, cached_pct,
                 cost.cache_creation_tokens, cost.total_cost, latency_ms, http_code);
        
        insert_request(model, cost.prompt_tokens, cost.completion_tokens, cost.total_tokens,
                      cost.cached_tokens, cost.cache_creation_tokens, cost.uncached_input_tokens, cost.output_tokens,
                      cost.input_cost, cost.output_cost, cost.cache_read_cost, cost.cache_write_cost, cost.total_cost,
                      0, temperature, max_tokens, latency_ms, http_code,
                      NULL, request_messages_json, response_content);
        
        // Update globals
        strncpy(g_last_model, model, sizeof(g_last_model) - 1);
        strncpy(g_last_status, "ok", sizeof(g_last_status) - 1);
        g_last_cost_usd = cost.total_cost;
        g_total_requests++;
        
        send_response(client_fd, http_code, "application/json", ctx.buffer);
        free(ctx.buffer);
        json_object_put(resp_json);
    } else {
        // Streaming: already sent, just log
        log_proxy("model=%s [stream] latency=%dms", model, latency_ms);
        g_total_requests++;
    }
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    json_object_put(json_body);
}

/* ── Usage Endpoints (port of dashscope-proxy.js:422-488) ─────────────────── */

static void handle_usage_summary(int client_fd) {
    pthread_mutex_lock(&g_db_mutex);
    
    const char *sql = 
        "SELECT "
        "  COUNT(*) as total_requests, "
        "  COALESCE(SUM(prompt_tokens), 0) as total_input_tokens, "
        "  COALESCE(SUM(completion_tokens), 0) as total_output_tokens, "
        "  COALESCE(SUM(cached_tokens), 0) as total_cached_tokens, "
        "  COALESCE(SUM(cache_creation_tokens), 0) as total_cache_creation_tokens, "
        "  COALESCE(SUM(total_cost), 0) as total_cost, "
        "  COALESCE(SUM(cache_read_cost), 0) as total_cache_read_cost, "
        "  COALESCE(SUM(cache_write_cost), 0) as total_cache_write_cost, "
        "  COUNT(DISTINCT model) as unique_models, "
        "  COALESCE(AVG(latency_ms), 0) as avg_latency_ms "
        "FROM requests";
    
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_mutex);
        send_response(client_fd, 500, "application/json", "{\"error\":\"DB error\"}");
        return;
    }
    
    json_object *json = json_object_new_object();
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        json_object_object_add(json, "total_requests", json_object_new_int64(sqlite3_column_int64(stmt, 0)));
        json_object_object_add(json, "total_input_tokens", json_object_new_int64(sqlite3_column_int64(stmt, 1)));
        json_object_object_add(json, "total_output_tokens", json_object_new_int64(sqlite3_column_int64(stmt, 2)));
        json_object_object_add(json, "total_cached_tokens", json_object_new_int64(sqlite3_column_int64(stmt, 3)));
        json_object_object_add(json, "total_cache_creation_tokens", json_object_new_int64(sqlite3_column_int64(stmt, 4)));
        json_object_object_add(json, "total_cost", json_object_new_double(sqlite3_column_double(stmt, 5)));
        json_object_object_add(json, "total_cache_read_cost", json_object_new_double(sqlite3_column_double(stmt, 6)));
        json_object_object_add(json, "total_cache_write_cost", json_object_new_double(sqlite3_column_double(stmt, 7)));
        json_object_object_add(json, "unique_models", json_object_new_int(sqlite3_column_int(stmt, 8)));
        json_object_object_add(json, "avg_latency_ms", json_object_new_double(sqlite3_column_double(stmt, 9)));
        
        double cache_hit_rate = sqlite3_column_int64(stmt, 1) > 0 ?
            (sqlite3_column_int64(stmt, 3) * 100.0 / sqlite3_column_int64(stmt, 1)) : 0.0;
        json_object_object_add(json, "cache_hit_rate", json_object_new_double(cache_hit_rate));
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_mutex);
    
    send_json(client_fd, 200, json);
}

static void handle_usage_daily(int client_fd, int days) {
    pthread_mutex_lock(&g_db_mutex);
    
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT "
        "  date(timestamp) as day, "
        "  COUNT(*) as request_count, "
        "  SUM(prompt_tokens) as input_tokens, "
        "  SUM(completion_tokens) as output_tokens, "
        "  SUM(cached_tokens) as cached_tokens, "
        "  SUM(cache_creation_tokens) as cache_creation_tokens, "
        "  SUM(total_cost) as total_cost "
        "FROM requests "
        "WHERE timestamp >= date('now', '-%d days') "
        "GROUP BY day "
        "ORDER BY day ASC", days);
    
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_mutex);
        send_response(client_fd, 500, "application/json", "{\"error\":\"DB error\"}");
        return;
    }
    
    json_object *json = json_object_new_array();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        json_object *row = json_object_new_object();
        json_object_object_add(row, "day", json_object_new_string((const char *)sqlite3_column_text(stmt, 0)));
        json_object_object_add(row, "request_count", json_object_new_int64(sqlite3_column_int64(stmt, 1)));
        json_object_object_add(row, "input_tokens", json_object_new_int64(sqlite3_column_int64(stmt, 2)));
        json_object_object_add(row, "output_tokens", json_object_new_int64(sqlite3_column_int64(stmt, 3)));
        json_object_object_add(row, "cached_tokens", json_object_new_int64(sqlite3_column_int64(stmt, 4)));
        json_object_object_add(row, "cache_creation_tokens", json_object_new_int64(sqlite3_column_int64(stmt, 5)));
        json_object_object_add(row, "total_cost", json_object_new_double(sqlite3_column_double(stmt, 6)));
        json_object_array_add(json, row);
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_mutex);
    
    send_json(client_fd, 200, json);
}

static void handle_usage_view(int client_fd, const char *view_name, const char *where_clause) {
    pthread_mutex_lock(&g_db_mutex);
    
    char sql[512];
    if (where_clause)
        snprintf(sql, sizeof(sql), "SELECT * FROM %s WHERE %s", view_name, where_clause);
    else
        snprintf(sql, sizeof(sql), "SELECT * FROM %s", view_name);
    
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_mutex);
        send_response(client_fd, 500, "application/json", "{\"error\":\"DB error\"}");
        return;
    }
    
    json_object *json = json_object_new_array();
    int cols = sqlite3_column_count(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        json_object *row = json_object_new_object();
        for (int i = 0; i < cols; i++) {
            const char *col_name = sqlite3_column_name(stmt, i);
            int col_type = sqlite3_column_type(stmt, i);
            
            switch (col_type) {
                case SQLITE_INTEGER:
                    json_object_object_add(row, col_name, json_object_new_int64(sqlite3_column_int64(stmt, i)));
                    break;
                case SQLITE_FLOAT:
                    json_object_object_add(row, col_name, json_object_new_double(sqlite3_column_double(stmt, i)));
                    break;
                case SQLITE_TEXT:
                    json_object_object_add(row, col_name, json_object_new_string((const char *)sqlite3_column_text(stmt, i)));
                    break;
                case SQLITE_NULL:
                    json_object_object_add(row, col_name, NULL);
                    break;
            }
        }
        json_object_array_add(json, row);
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_mutex);
    
    send_json(client_fd, 200, json);
}

static void handle_usage_requests(int client_fd, int limit, const char *model_filter) {
    pthread_mutex_lock(&g_db_mutex);
    
    sqlite3_stmt *stmt;
    const char *sql;
    
    if (model_filter) {
        sql = "SELECT id, timestamp, model, prompt_tokens, completion_tokens, cached_tokens, "
              "  cache_creation_tokens, total_cost, stream, latency_ms, status_code "
              "FROM requests WHERE model = ? ORDER BY id DESC LIMIT ?";
        if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
            pthread_mutex_unlock(&g_db_mutex);
            send_response(client_fd, 500, "application/json", "{\"error\":\"DB error\"}");
            return;
        }
        sqlite3_bind_text(stmt, 1, model_filter, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, limit);
    } else {
        sql = "SELECT id, timestamp, model, prompt_tokens, completion_tokens, cached_tokens, "
              "  cache_creation_tokens, total_cost, stream, latency_ms, status_code "
              "FROM requests ORDER BY id DESC LIMIT ?";
        if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
            pthread_mutex_unlock(&g_db_mutex);
            send_response(client_fd, 500, "application/json", "{\"error\":\"DB error\"}");
            return;
        }
        sqlite3_bind_int(stmt, 1, limit);
    }
    
    json_object *json = json_object_new_array();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        json_object *row = json_object_new_object();
        json_object_object_add(row, "id", json_object_new_int64(sqlite3_column_int64(stmt, 0)));
        json_object_object_add(row, "timestamp", json_object_new_string((const char *)sqlite3_column_text(stmt, 1)));
        json_object_object_add(row, "model", json_object_new_string((const char *)sqlite3_column_text(stmt, 2)));
        json_object_object_add(row, "prompt_tokens", json_object_new_int64(sqlite3_column_int64(stmt, 3)));
        json_object_object_add(row, "completion_tokens", json_object_new_int64(sqlite3_column_int64(stmt, 4)));
        json_object_object_add(row, "cached_tokens", json_object_new_int64(sqlite3_column_int64(stmt, 5)));
        json_object_object_add(row, "cache_creation_tokens", json_object_new_int64(sqlite3_column_int64(stmt, 6)));
        json_object_object_add(row, "total_cost", json_object_new_double(sqlite3_column_double(stmt, 7)));
        json_object_object_add(row, "stream", json_object_new_int(sqlite3_column_int(stmt, 8)));
        json_object_object_add(row, "latency_ms", json_object_new_int(sqlite3_column_int(stmt, 9)));
        json_object_object_add(row, "status_code", json_object_new_int(sqlite3_column_int(stmt, 10)));
        json_object_array_add(json, row);
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_mutex);
    
    send_json(client_fd, 200, json);
}

/* ── Connection Handler ───────────────────────────────────────────────────── */

static void *connection_thread(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);
    
    g_active_connections++;
    
    HttpRequest req;
    if (read_http_request(client_fd, &req) < 0) {
        close(client_fd);
        g_active_connections--;
        return NULL;
    }
    
    // Parse path and query string
    char *query = strchr(req.path, '?');
    char path[2048];
    if (query) {
        int path_len = query - req.path;
        strncpy(path, req.path, path_len);
        path[path_len] = '\0';
        query++;
    } else {
        strncpy(path, req.path, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    }
    
    // Route request
    if (strcmp(path, "/health") == 0) {
        json_object *json = json_object_new_object();
        json_object_object_add(json, "status", json_object_new_string("ok"));
        json_object_object_add(json, "upstream", json_object_new_string(UPSTREAM_URL));
        json_object_object_add(json, "db", json_object_new_string(DB_PATH));
        json_object_object_add(json, "models", json_object_new_int(PRICING_COUNT));
        send_json(client_fd, 200, json);
    }
    else if (strcmp(path, "/v1/chat/completions") == 0 && strcmp(req.method, "POST") == 0) {
        handle_chat_completions(client_fd, req.body, req.body_len);
    }
    else if (strncmp(path, "/usage/", 7) == 0) {
        if (strcmp(path, "/usage/summary") == 0) {
            handle_usage_summary(client_fd);
        }
        else if (strcmp(path, "/usage/daily") == 0) {
            int days = 30;
            if (query) {
                char *days_param = strstr(query, "days=");
                if (days_param) {
                    sscanf(days_param, "days=%d", &days);
                }
            }
            handle_usage_daily(client_fd, days);
        }
        else if (strcmp(path, "/usage/today") == 0) {
            handle_usage_view(client_fd, "v_daily_summary", "day = date('now')");
        }
        else if (strcmp(path, "/usage/monthly") == 0) {
            handle_usage_view(client_fd, "v_monthly_summary", "month = strftime('%Y-%m', 'now')");
        }
        else if (strcmp(path, "/usage/total") == 0) {
            handle_usage_view(client_fd, "v_total_summary", NULL);
        }
        else if (strcmp(path, "/usage/requests") == 0) {
            int limit = 50;
            const char *model_filter = NULL;
            
            if (query) {
                char *limit_param = strstr(query, "limit=");
                if (limit_param) {
                    sscanf(limit_param, "limit=%d", &limit);
                    if (limit > 500) limit = 500;
                }
                
                char *model_param = strstr(query, "model=");
                if (model_param) {
                    model_filter = model_param + 6;
                    char *amp = strchr(model_filter, '&');
                    if (amp) *amp = '\0';
                }
            }
            
            handle_usage_requests(client_fd, limit, model_filter);
        }
        else {
            send_response(client_fd, 404, "application/json", "{\"error\":\"Not found\"}");
        }
    }
    else if (strcmp(path, "/dashboard") == 0 || strcmp(path, "/") == 0) {
        send_response(client_fd, 404, "text/plain", "Dashboard moved to GTK tab");
    }
    else {
        send_response(client_fd, 404, "application/json", "{\"error\":\"Not found\"}");
    }
    
    if (req.body) free(req.body);
    close(client_fd);
    g_active_connections--;
    
    return NULL;
}

/* ── Listener Thread ──────────────────────────────────────────────────────── */

static void *listener_thread(void *arg) {
    (void)arg;
    
    while (g_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(g_listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (g_running) {
                log_proxy("Accept error: %s", strerror(errno));
            }
            continue;
        }
        
        // Set socket timeout
        struct timeval tv;
        tv.tv_sec = SOCKET_TIMEOUT;
        tv.tv_usec = 0;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        
        if (g_active_connections >= MAX_CONNECTIONS) {
            send_response(client_fd, 503, "text/plain", "Too many connections");
            close(client_fd);
            continue;
        }
        
        int *fd_ptr = malloc(sizeof(int));
        *fd_ptr = client_fd;
        
        pthread_t thread;
        if (pthread_create(&thread, NULL, connection_thread, fd_ptr) != 0) {
            log_proxy("Thread creation failed");
            close(client_fd);
            free(fd_ptr);
        } else {
            pthread_detach(thread);
        }
    }
    
    return NULL;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

void proxy_init(void) {
    // Read API key from ~/.omp/.env.proxy
    const char *home = getenv("HOME");
    if (!home) {
        log_proxy("HOME not set");
        return;
    }
    
    char env_path[512];
    snprintf(env_path, sizeof(env_path), "%s/.omp/.env.proxy", home);
    
    FILE *f = fopen(env_path, "r");
    if (!f) {
        log_proxy("Cannot open %s", env_path);
        return;
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "DASHSCOPE_API_KEY=", 18) == 0) {
            char *value = line + 18;
            char *newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            strncpy(g_api_key, value, sizeof(g_api_key) - 1);
            break;
        }
    }
    fclose(f);
    
    if (strlen(g_api_key) == 0) {
        log_proxy("DASHSCOPE_API_KEY not found");
        return;
    }
    
    // Open database
    if (sqlite3_open(DB_PATH, &g_db) != SQLITE_OK) {
        log_proxy("Cannot open database: %s", sqlite3_errmsg(g_db));
        g_db = NULL;
        return;
    }
    
    sqlite3_exec(g_db, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);
    sqlite3_exec(g_db, "PRAGMA synchronous=NORMAL", NULL, NULL, NULL);
    sqlite3_exec(g_db, "PRAGMA busy_timeout=5000", NULL, NULL, NULL);
    
    if (db_setup() < 0) {
        sqlite3_close(g_db);
        g_db = NULL;
        return;
    }
    
    // Create socket
    g_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_listen_fd < 0) {
        log_proxy("Socket creation failed");
        sqlite3_close(g_db);
        g_db = NULL;
        return;
    }
    
    int opt = 1;
    setsockopt(g_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PROXY_PORT);
    addr.sin_addr.s_addr = inet_addr(PROXY_BIND);
    
    if (bind(g_listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_proxy("Bind failed: %s", strerror(errno));
        close(g_listen_fd);
        g_listen_fd = -1;
        sqlite3_close(g_db);
        g_db = NULL;
        return;
    }
    
    if (listen(g_listen_fd, 10) < 0) {
        log_proxy("Listen failed");
        close(g_listen_fd);
        g_listen_fd = -1;
        sqlite3_close(g_db);
        g_db = NULL;
        return;
    }
    
    g_running = 1;
    g_started_at = time(NULL);
    
    if (pthread_create(&g_listener_thread, NULL, listener_thread, NULL) != 0) {
        log_proxy("Listener thread creation failed");
        close(g_listen_fd);
        g_listen_fd = -1;
        sqlite3_close(g_db);
        g_db = NULL;
        g_running = 0;
        return;
    }
    
    log_proxy("DashScope proxy listening on http://%s:%d", PROXY_BIND, PROXY_PORT);
    log_proxy("Upstream: %s", UPSTREAM_URL);
    log_proxy("DB: %s", DB_PATH);
    log_proxy("Models in pricing map: %d", PRICING_COUNT);
}

void proxy_shutdown(void) {
    if (!g_running) return;
    
    g_running = 0;
    
    if (g_listen_fd >= 0) {
        shutdown(g_listen_fd, SHUT_RDWR);
        close(g_listen_fd);
        g_listen_fd = -1;
    }
    
    pthread_join(g_listener_thread, NULL);
    
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
    
    log_proxy("Proxy shutdown complete");
}

ProxyStatus proxy_get_status(void) {
    ProxyStatus status = {0};
    status.running = g_running;
    status.port = PROXY_PORT;
    status.started_at = g_started_at;
    status.total_requests = g_total_requests;
    status.active_connections = g_active_connections;
    status.error_count = g_error_count;
    strncpy(status.last_model, g_last_model, sizeof(status.last_model) - 1);
    strncpy(status.last_status, g_last_status, sizeof(status.last_status) - 1);
    status.last_cost_usd = g_last_cost_usd;
    return status;
}

/* ── Query Helpers for UI ─────────────────────────────────────────────────── */

ProxyModelRow *proxy_query_summary(int *out_count) {
    if (!g_db) {
        *out_count = 0;
        return NULL;
    }
    
    pthread_mutex_lock(&g_db_mutex);
    
    const char *sql = "SELECT * FROM v_total_summary";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_mutex);
        *out_count = 0;
        return NULL;
    }
    
    int capacity = 32;
    ProxyModelRow *rows = malloc(sizeof(ProxyModelRow) * capacity);
    int count = 0;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            rows = realloc(rows, sizeof(ProxyModelRow) * capacity);
        }
        
        strncpy(rows[count].model, (const char *)sqlite3_column_text(stmt, 0), sizeof(rows[count].model) - 1);
        rows[count].input_tokens = sqlite3_column_int64(stmt, 1);
        rows[count].output_tokens = sqlite3_column_int64(stmt, 2);
        rows[count].cached = sqlite3_column_int64(stmt, 3);
        rows[count].cache_create = sqlite3_column_int64(stmt, 4);
        rows[count].total_cost = sqlite3_column_double(stmt, 5);
        rows[count].request_count = sqlite3_column_int64(stmt, 6);
        count++;
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_mutex);
    
    *out_count = count;
    return rows;
}

ProxyModelRow *proxy_query_daily_models(const char *day, int *out_count) {
    if (!g_db) {
        *out_count = 0;
        return NULL;
    }
    
    pthread_mutex_lock(&g_db_mutex);
    
    const char *sql = "SELECT model, input_tokens, output_tokens, cached_tokens, cache_creation_tokens, total_cost, request_count "
                     "FROM v_daily_summary WHERE day = ? ORDER BY total_cost DESC";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_mutex);
        *out_count = 0;
        return NULL;
    }
    
    sqlite3_bind_text(stmt, 1, day, -1, SQLITE_TRANSIENT);
    
    int capacity = 32;
    ProxyModelRow *rows = malloc(sizeof(ProxyModelRow) * capacity);
    int count = 0;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            rows = realloc(rows, sizeof(ProxyModelRow) * capacity);
        }
        
        strncpy(rows[count].model, (const char *)sqlite3_column_text(stmt, 0), sizeof(rows[count].model) - 1);
        rows[count].input_tokens = sqlite3_column_int64(stmt, 1);
        rows[count].output_tokens = sqlite3_column_int64(stmt, 2);
        rows[count].cached = sqlite3_column_int64(stmt, 3);
        rows[count].cache_create = sqlite3_column_int64(stmt, 4);
        rows[count].total_cost = sqlite3_column_double(stmt, 5);
        rows[count].request_count = sqlite3_column_int64(stmt, 6);
        count++;
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_mutex);
    
    *out_count = count;
    return rows;
}

ProxyModelRow *proxy_query_monthly(const char *ym, int *out_count) {
    if (!g_db) {
        *out_count = 0;
        return NULL;
    }
    
    pthread_mutex_lock(&g_db_mutex);
    
    const char *sql = "SELECT model, input_tokens, output_tokens, cached_tokens, cache_creation_tokens, total_cost, request_count "
                     "FROM v_monthly_summary WHERE month = ? ORDER BY total_cost DESC";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_mutex);
        *out_count = 0;
        return NULL;
    }
    
    sqlite3_bind_text(stmt, 1, ym, -1, SQLITE_TRANSIENT);
    
    int capacity = 32;
    ProxyModelRow *rows = malloc(sizeof(ProxyModelRow) * capacity);
    int count = 0;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            rows = realloc(rows, sizeof(ProxyModelRow) * capacity);
        }
        
        strncpy(rows[count].model, (const char *)sqlite3_column_text(stmt, 0), sizeof(rows[count].model) - 1);
        rows[count].input_tokens = sqlite3_column_int64(stmt, 1);
        rows[count].output_tokens = sqlite3_column_int64(stmt, 2);
        rows[count].cached = sqlite3_column_int64(stmt, 3);
        rows[count].cache_create = sqlite3_column_int64(stmt, 4);
        rows[count].total_cost = sqlite3_column_double(stmt, 5);
        rows[count].request_count = sqlite3_column_int64(stmt, 6);
        count++;
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_mutex);
    
    *out_count = count;
    return rows;
}

ProxyRequestRow *proxy_query_recent(int limit, int *out_count) {
    if (!g_db) {
        *out_count = 0;
        return NULL;
    }
    
    pthread_mutex_lock(&g_db_mutex);
    
    const char *sql = "SELECT id, timestamp, model, prompt_tokens, completion_tokens, "
                      "cached_tokens, cache_creation_tokens, total_cost, stream, latency_ms, status_code "
                      "FROM requests ORDER BY id DESC LIMIT ?";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_mutex);
        *out_count = 0;
        return NULL;
    }
    
    sqlite3_bind_int(stmt, 1, limit);
    
    int capacity = limit > 32 ? limit : 32;
    ProxyRequestRow *rows = malloc(sizeof(ProxyRequestRow) * capacity);
    int count = 0;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        rows[count].id = sqlite3_column_int64(stmt, 0);
        strncpy(rows[count].timestamp, (const char *)sqlite3_column_text(stmt, 1), sizeof(rows[count].timestamp) - 1);
        strncpy(rows[count].model, (const char *)sqlite3_column_text(stmt, 2), sizeof(rows[count].model) - 1);
        rows[count].prompt = sqlite3_column_int64(stmt, 3);
        rows[count].completion = sqlite3_column_int64(stmt, 4);
        rows[count].cached = sqlite3_column_int64(stmt, 5);
        rows[count].cache_create = sqlite3_column_int64(stmt, 6);
        rows[count].cost = sqlite3_column_double(stmt, 7);
        rows[count].stream = sqlite3_column_int(stmt, 8);
        rows[count].latency_ms = sqlite3_column_int(stmt, 9);
        rows[count].status_code = sqlite3_column_int(stmt, 10);
        count++;
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_mutex);
    
    *out_count = count;
    return rows;
}

ProxyRequestDetail *proxy_query_request_detail(long id) {
    if (!g_db) return NULL;
    
    pthread_mutex_lock(&g_db_mutex);
    
    const char *sql = "SELECT id, timestamp, model, prompt_tokens, completion_tokens, "
                      "total_tokens, cached_tokens, cache_creation_tokens, uncached_input_tokens, "
                      "output_tokens, input_cost, output_cost, cache_read_cost, cache_write_cost, "
                      "total_cost, stream, latency_ms, status_code, request_messages, response_content "
                      "FROM requests WHERE id = ?";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_mutex);
        return NULL;
    }
    
    sqlite3_bind_int64(stmt, 1, id);
    
    ProxyRequestDetail *detail = NULL;
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        detail = malloc(sizeof(ProxyRequestDetail));
        if (!detail) {
            sqlite3_finalize(stmt);
            pthread_mutex_unlock(&g_db_mutex);
            return NULL;
        }
        
        detail->id = sqlite3_column_int64(stmt, 0);
        strncpy(detail->timestamp, (const char *)sqlite3_column_text(stmt, 1), sizeof(detail->timestamp) - 1);
        detail->timestamp[sizeof(detail->timestamp) - 1] = '\0';
        strncpy(detail->model, (const char *)sqlite3_column_text(stmt, 2), sizeof(detail->model) - 1);
        detail->model[sizeof(detail->model) - 1] = '\0';
        detail->prompt = sqlite3_column_int64(stmt, 3);
        detail->completion = sqlite3_column_int64(stmt, 4);
        detail->total_tokens = sqlite3_column_int64(stmt, 5);
        detail->cached = sqlite3_column_int64(stmt, 6);
        detail->cache_create = sqlite3_column_int64(stmt, 7);
        detail->uncached_input = sqlite3_column_int64(stmt, 8);
        detail->output_tokens = sqlite3_column_int64(stmt, 9);
        detail->input_cost = sqlite3_column_double(stmt, 10);
        detail->output_cost = sqlite3_column_double(stmt, 11);
        detail->cache_read_cost = sqlite3_column_double(stmt, 12);
        detail->cache_write_cost = sqlite3_column_double(stmt, 13);
        detail->cost = sqlite3_column_double(stmt, 14);
        detail->stream = sqlite3_column_int(stmt, 15);
        detail->latency_ms = sqlite3_column_int(stmt, 16);
        detail->status_code = sqlite3_column_int(stmt, 17);
        
        const unsigned char *req_msg = sqlite3_column_text(stmt, 18);
        const unsigned char *resp_content = sqlite3_column_text(stmt, 19);
        
        detail->request_messages = req_msg ? strdup((const char *)req_msg) : NULL;
        detail->response_content = resp_content ? strdup((const char *)resp_content) : NULL;
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_mutex);
    
    return detail;
}

void proxy_free_request_detail(ProxyRequestDetail *detail) {
    if (detail) {
        if (detail->request_messages) free(detail->request_messages);
        if (detail->response_content) free(detail->response_content);
        free(detail);
    }
}


ProxyDailyRow *proxy_query_daily_series(int days, int *out_count) {
    if (!g_db) {
        *out_count = 0;
        return NULL;
    }
    
    pthread_mutex_lock(&g_db_mutex);
    
    const char *sql = "SELECT date(timestamp) as day, COUNT(*) as request_count, "
                      "SUM(prompt_tokens) as input_tokens, SUM(completion_tokens) as output_tokens, "
                      "SUM(cached_tokens) as cached_tokens, SUM(total_cost) as total_cost "
                      "FROM requests WHERE timestamp >= date('now', '-' || ? || ' days') "
                      "GROUP BY day ORDER BY day ASC";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_mutex);
        *out_count = 0;
        return NULL;
    }
    
    sqlite3_bind_int(stmt, 1, days);
    
    int capacity = days > 32 ? days : 32;
    ProxyDailyRow *rows = malloc(sizeof(ProxyDailyRow) * capacity);
    int count = 0;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        strncpy(rows[count].day, (const char *)sqlite3_column_text(stmt, 0), sizeof(rows[count].day) - 1);
        rows[count].request_count = sqlite3_column_int64(stmt, 1);
        rows[count].input_tokens = sqlite3_column_int64(stmt, 2);
        rows[count].output_tokens = sqlite3_column_int64(stmt, 3);
        rows[count].cached_tokens = sqlite3_column_int64(stmt, 4);
        rows[count].total_cost = sqlite3_column_double(stmt, 5);
        count++;
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_mutex);
    
    *out_count = count;
    return rows;
}

void proxy_free_rows(void *rows) {
    free(rows);
}

#else /* _WIN32 */
/* Stub implementations for Windows - proxy disabled */
#include "proxy.h"
#include <stdio.h>

void proxy_init(void) {
    fprintf(stderr, "[proxy] Disabled on Windows\n");
}

void proxy_shutdown(void) {
    /* no-op */
}

ProxyStatus proxy_get_status(void) {
    ProxyStatus status = {0};
    status.running = 0;
    return status;
}

ProxyModelRow *proxy_query_summary(int *out_count) {
    *out_count = 0;
    return NULL;
}

ProxyModelRow *proxy_query_daily_models(const char *day, int *out_count) {
    (void)day;
    *out_count = 0;
    return NULL;
}

ProxyModelRow *proxy_query_monthly(const char *ym, int *out_count) {
    (void)ym;
    *out_count = 0;
    return NULL;
}

ProxyRequestRow *proxy_query_recent(int limit, int *out_count) {
    (void)limit;
    *out_count = 0;
    return NULL;
}

ProxyDailyRow *proxy_query_daily_series(int days, int *out_count) {
    (void)days;
    *out_count = 0;
    return NULL;
}

void proxy_free_rows(void *rows) {
    free(rows);
}

ProxyRequestDetail *proxy_query_request_detail(long id) {
    (void)id;
    return NULL;
}

void proxy_free_request_detail(ProxyRequestDetail *detail) {
    if (detail) {
        if (detail->request_messages) free(detail->request_messages);
        if (detail->response_content) free(detail->response_content);
        free(detail);
    }
}
#endif /* _WIN32 */