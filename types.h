#ifndef TYPES_H
#define TYPES_H

#include <gtk/gtk.h>
#include "config.h"

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
} DailyUsage;

typedef struct {
    char model[128];
    char region[64];
    DailyUsage daily[31];
    int day_count;
    double total_cost;
    double total_tokens;
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
} ModelsCardWidgets;

typedef struct {
    GtkWidget *history_box;
    GtkWidget *last_updated_label;
    GtkWidget *spinner;
    GtkWidget *card_revealer;
} HistoricalCardWidgets;

typedef struct {
    char name[256];
    char coupon_no[64];
    char type[64];
    char status[32];
    double amount;
    double remaining;
    char expiry[64];
} CouponItem;

typedef struct {
    CouponItem items[MAX_ITEMS];
    int count;
    double total_remaining;
    double total_amount;
    gboolean success;
    char error[256];
} CouponData;

typedef struct {
    GtkWidget *total_label;
    GtkWidget *cash_label;
    GtkWidget *coupons_box;
    GtkWidget *last_updated_label;
    GtkWidget *spinner;
    GtkWidget *card_revealer;
} CouponCardWidgets;

#endif