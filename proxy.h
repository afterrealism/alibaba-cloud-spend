#ifndef PROXY_H
#define PROXY_H

#include <glib.h>
#include <time.h>

typedef struct {
    gboolean running;
    int port;
    time_t started_at;
    long total_requests;
    long active_connections;
    long error_count;
    char last_model[128];
    char last_status[32];
    double last_cost_usd;
} ProxyStatus;

typedef struct ProxyModelRow {
    char model[128];
    long input_tokens;
    long output_tokens;
    long cached;
    long cache_create;
    long request_count;
    double total_cost;
} ProxyModelRow;


typedef struct {
    long id;
    char timestamp[32];
    char model[128];
    long prompt;
    long completion;
    long cached;
    long cache_create;
    double cost;
    int stream;
    int latency_ms;
    int status_code;
} ProxyRequestRow;

typedef struct {
    long id;
    char timestamp[32];
    char model[128];
    long prompt;
    long completion;
    long total_tokens;
    long cached;
    long cache_create;
    long uncached_input;
    long output_tokens;
    double input_cost;
    double output_cost;
    double cache_read_cost;
    double cache_write_cost;
    double cost;
    int stream;
    int latency_ms;
    int status_code;
    char *request_messages;   // JSON text (malloc'd)
    char *response_content;   // JSON text (malloc'd)
} ProxyRequestDetail;

ProxyRequestDetail *proxy_query_request_detail(long id);
void proxy_free_request_detail(ProxyRequestDetail *detail);

typedef struct ProxyDailyRow {
    char day[16];
    long request_count;
    long input_tokens;
    long output_tokens;
    long cached_tokens;
    double total_cost;
} ProxyDailyRow;

void proxy_init(void);
void proxy_shutdown(void);
ProxyStatus proxy_get_status(void);

ProxyModelRow *proxy_query_summary(int *out_count);
ProxyModelRow *proxy_query_daily_models(const char *day, int *out_count);
ProxyModelRow *proxy_query_monthly(const char *ym, int *out_count);
ProxyRequestRow *proxy_query_recent(int limit, int *out_count);
ProxyDailyRow *proxy_query_daily_series(int days, int *out_count);
void proxy_free_rows(void *rows);

#endif
