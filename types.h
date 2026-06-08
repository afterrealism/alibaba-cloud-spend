#ifndef TYPES_H
#define TYPES_H

#include <gtk/gtk.h>
#include "config.h"

// Forward declarations for proxy types
typedef struct ProxyModelRow ProxyModelRow;
typedef struct ProxyDailyRow ProxyDailyRow;


typedef struct {
    char product[128];
    double amount;
} BillItem;

typedef struct {
    BillItem items[MAX_ITEMS];
    int count;
    double total;
    gboolean success;
    char error[256];
} BillingData;

typedef struct {
    char name[128];
    double amount;
} ServiceSummary;

typedef struct {
    char model[128];
    char region[64];
    double amount;
} ModelSummary;

typedef struct {
    ServiceSummary services[MAX_ITEMS];
    int service_count;
    ModelSummary models[MAX_ITEMS];
    int model_count;
    double total_spend;
    gboolean success;
    char error[256];
} DetailedBillingData;

typedef struct {
    char service[128];
    char month[16];
    double amount;
} HistoricalEntry;

typedef struct {
    HistoricalEntry entries[MAX_ITEMS * 6];
    int count;
    gboolean success;
    char error[256];
} HistoricalBillingData;

typedef struct {
    GtkWidget *total_label;
    GtkWidget *breakdown_box;
    GtkWidget *last_updated_label;
    GtkWidget *spinner;
    GtkWidget *card_revealer;
} BillingCardWidgets;

typedef struct {
    GtkWidget *total_label;
    GtkWidget *services_box;
    GtkWidget *last_updated_label;
    GtkWidget *spinner;
    GtkWidget *card_revealer;
} ServicesCardWidgets;

typedef struct {
    char date[16];
    double cost;
    double tokens;
    long input_tokens;
    long output_tokens;
} DailyUsage;

typedef struct {
    char model[128];
    char region[64];
    DailyUsage daily[31];
    int day_count;
    double total_cost;
    double total_tokens;
    long total_input_tokens;
    long total_output_tokens;
    gboolean success;
    char error[256];
} ModelDetailData;

typedef struct {
    GtkWidget *total_label;
    GtkWidget *models_box;
    GtkWidget *last_updated_label;
    GtkWidget *spinner;
    GtkWidget *card_revealer;
    GtkWidget *detail_box;
    GtkWidget *back_button;
    GtkWidget *detail_title;
    GtkWidget *detail_content;
    gboolean showing_detail;
    char region_filter[64];
    ModelDetailData detail_data;
} ModelsCardWidgets;

typedef struct {
    GtkWidget *history_box;
    GtkWidget *last_updated_label;
    GtkWidget *spinner;
    GtkWidget *card_revealer;
} HistoricalCardWidgets;

typedef struct {
    GtkWidget *status_dot;
    GtkWidget *status_text;
    GtkWidget *uptime_label;
    GtkWidget *rps_label;
    GtkWidget *summary_total_cost;
    GtkWidget *summary_requests;
    GtkWidget *summary_cache_hit;
    GtkWidget *summary_tokens;
    GtkWidget *chart_doughnut;
    GtkWidget *chart_cache_line;
    GtkWidget *chart_daily_bar;
    GtkWidget *today_box;
    GtkWidget *monthly_box;
    GtkWidget *recent_box;
    GtkWidget *load_more_btn;
    int recent_limit;
    GtkWidget *open_dashboard_btn;
    GtkWidget *show_log_btn;
    GtkWidget *last_updated_label;
    GtkWidget *spinner;
    GtkWidget *card_revealer;
    ProxyModelRow *chart_models;
    int chart_models_count;
    ProxyDailyRow *chart_daily;
    int chart_daily_count;
    GtkWidget *detail_box;
    GtkWidget *detail_content;
    gboolean showing_detail;
} ProxyCardWidgets;


#endif