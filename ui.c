#include "ui.h"
#include "api.h"
#include "config.h"
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define ALIBABA_ORANGE   "#FF6A00"
#define ALIBABA_ACCENT   "#FF8133"
#define BG_DARK          "#1C1C1C"
#define CARD_BG          "#252525"
#define TEXT_PRIMARY      "#D0D0D0"
#define TEXT_SECONDARY    "#AAAAAA"
#define TEXT_MUTED        "#777777"
#define BORDER_SUBTLE     "#333333"
#define DANGER_RED        "#FF4D4F"
#define SUCCESS_GREEN     "#52c41a"

static BillingCardWidgets billing_card;
static ServicesCardWidgets services_card;
static CouponCardWidgets coupons_card;
static ModelsCardWidgets models_beijing_card;
static ModelsCardWidgets models_singapore_card;
static HistoricalCardWidgets historical_card;

/* ── CSS styling ─────────────────────────────────────────────────── */

void apply_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    const char *css =
        "window { background-color: " BG_DARK "; }"
        ".card {"
        "  background-color: " CARD_BG ";"
        "  border-radius: 12px;"
        "  padding: 24px;"
        "  border: 1px solid " BORDER_SUBTLE ";"
        "}"
        "notebook {"
        "  background-color: " BG_DARK ";"
        "  border: none;"
        "}"
        "notebook tab {"
        "  background-color: " CARD_BG ";"
        "  color: " TEXT_SECONDARY ";"
        "  padding: 8px 16px;"
        "  border-radius: 8px 8px 0 0;"
        "  border: 1px solid " BORDER_SUBTLE ";"
        "  border-bottom: none;"
        "  font-weight: bold;"
        "}"
        "notebook tab:checked {"
        "  background-color: " ALIBABA_ORANGE ";"
        "  color: " TEXT_PRIMARY ";"
        "}"
        "notebook header {"
        "  border: none;"
        "  background-color: " BG_DARK ";"
        "}"
        "notebook stack {"
        "  background-color: " BG_DARK ";"
        "  border: none;"
        "}"
        ".gear-btn {"
        "  padding: 2px 8px;"
        "  background: transparent;"
        "  border: none;"
        "}"
        ".gear-btn:hover {"
        "  background: rgba(255,255,255,0.08);"
        "  border-radius: 6px;"
        "}"
        ".gear-btn label {"
        "  font-size: 22px;"
        "  color: " TEXT_SECONDARY ";"
        "}"
        ".settings-window {"
        "  background-color: " BG_DARK ";"
        "}"
        ".settings-window entry {"
        "  background-color: " CARD_BG ";"
        "  color: " TEXT_PRIMARY ";"
        "  border: 1px solid " BORDER_SUBTLE ";"
        "  border-radius: 8px;"
        "  padding: 8px 12px;"
        "  min-height: 20px;"
        "}"
        ".settings-window entry:focus {"
        "  border-color: " ALIBABA_ORANGE ";"
        "}"
        ".save-btn {"
        "  background-image: none;"
        "  background-color: " ALIBABA_ORANGE ";"
        "  color: " TEXT_PRIMARY ";"
        "  border: 1px solid " BORDER_SUBTLE ";"
        "  border-radius: 8px;"
        "  padding: 8px 20px;"
        "  font-weight: bold;"
        "  box-shadow: none;"
        "}"
        ".save-btn:hover {"
        "  background-color: " ALIBABA_ACCENT ";"
        "}"
        ".cancel-btn {"
        "  background-image: none;"
        "  background-color: " CARD_BG ";"
        "  color: " TEXT_SECONDARY ";"
        "  border: 1px solid " BORDER_SUBTLE ";"
        "  border-radius: 8px;"
        "  padding: 8px 20px;"
        "  font-weight: bold;"
        "  box-shadow: none;"
        "}"
        ".cancel-btn:hover {"
        "  background-color: #444444;"
        "}";
    gtk_css_provider_load_from_string(provider, css);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

/* ── Helpers ─────────────────────────────────────────────────────── */

static void clear_box(GtkWidget *box) {
    GListModel *children = gtk_widget_observe_children(box);
    guint n = g_list_model_get_n_items(children);
    for (guint i = n; i > 0; i--) {
        GtkWidget *child = g_list_model_get_item(children, i - 1);
        if (child) {
            gtk_box_remove(GTK_BOX(box), child);
            g_object_unref(child);
        }
    }
    g_object_unref(children);
}

static void set_timestamp(GtkWidget *label) {
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%H:%M:%S", lt);
    char markup[128];
    snprintf(markup, sizeof(markup),
             "<span size='x-small' foreground='" TEXT_MUTED "'>Last updated: %s</span>", timebuf);
    gtk_label_set_markup(GTK_LABEL(label), markup);
}

static void add_row(GtkWidget *box, const char *name, double amount,
                    const char *name_color, const char *amt_color) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(row, TRUE);

    GtkWidget *lbl_name = gtk_label_new(NULL);
    char nm[384];
    snprintf(nm, sizeof(nm),
             "<span size='small' foreground='%s'>%s</span>", name_color, name);
    gtk_label_set_markup(GTK_LABEL(lbl_name), nm);
    gtk_label_set_wrap(GTK_LABEL(lbl_name), TRUE);
    gtk_widget_set_halign(lbl_name, GTK_ALIGN_START);
    gtk_widget_set_hexpand(lbl_name, TRUE);

    GtkWidget *lbl_amt = gtk_label_new(NULL);
    char am[256];
    snprintf(am, sizeof(am),
             "<span size='small' weight='bold' foreground='%s'>$%.2f</span>", amt_color, amount);
    gtk_label_set_markup(GTK_LABEL(lbl_amt), am);
    gtk_widget_set_halign(lbl_amt, GTK_ALIGN_END);

    gtk_box_append(GTK_BOX(row), lbl_name);
    gtk_box_append(GTK_BOX(row), lbl_amt);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_opacity(sep, 0.1);

    gtk_box_append(GTK_BOX(box), sep);
    gtk_box_append(GTK_BOX(box), row);
}

static int cmp_service_desc(const void *a, const void *b) {
    const ServiceSummary *sa = (const ServiceSummary *)a;
    const ServiceSummary *sb = (const ServiceSummary *)b;
    if (sb->amount > sa->amount) return 1;
    if (sb->amount < sa->amount) return -1;
    return 0;
}

static int cmp_model_desc(const void *a, const void *b) {
    const ModelSummary *ma = (const ModelSummary *)a;
    const ModelSummary *mb = (const ModelSummary *)b;
    if (mb->amount > ma->amount) return 1;
    if (mb->amount < ma->amount) return -1;
    return 0;
}

typedef struct {
    char model[128];
    char region[64];
    ModelsCardWidgets *widgets;
} ModelClickData;

static void on_model_row_clicked(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data);

static void add_clickable_model_row(GtkWidget *box, ModelsCardWidgets *w, const char *model_name, double amount,
                                     double pct, const char *region) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(row, TRUE);

    GtkWidget *lbl_name = gtk_label_new(NULL);
    char nm[384];
    snprintf(nm, sizeof(nm),
             "<span size='small' foreground='" TEXT_SECONDARY "'>%s  </span>"
             "<span size='x-small' foreground='" TEXT_MUTED "'>(%.0f%%)</span>"
             "<span size='x-small' foreground='" ALIBABA_ORANGE "'> ▶</span>",
             model_name, pct);
    gtk_label_set_markup(GTK_LABEL(lbl_name), nm);
    gtk_label_set_wrap(GTK_LABEL(lbl_name), TRUE);
    gtk_widget_set_halign(lbl_name, GTK_ALIGN_START);
    gtk_widget_set_hexpand(lbl_name, TRUE);

    GtkWidget *lbl_amt = gtk_label_new(NULL);
    char am[256];
    snprintf(am, sizeof(am),
             "<span size='small' weight='bold' foreground='" ALIBABA_ACCENT "'>$%.2f</span>", amount);
    gtk_label_set_markup(GTK_LABEL(lbl_amt), am);
    gtk_widget_set_halign(lbl_amt, GTK_ALIGN_END);

    gtk_box_append(GTK_BOX(row), lbl_name);
    gtk_box_append(GTK_BOX(row), lbl_amt);

    GtkGesture *click = gtk_gesture_click_new();
    ModelClickData *click_data = g_malloc0(sizeof(ModelClickData));
    strncpy(click_data->model, model_name, sizeof(click_data->model) - 1);
    strncpy(click_data->region, region, sizeof(click_data->region) - 1);
    click_data->widgets = w;
    g_signal_connect(click, "pressed", G_CALLBACK(on_model_row_clicked), click_data);
    g_object_set_data_full(G_OBJECT(row), "click-data", click_data, g_free);
    gtk_widget_add_controller(row, GTK_EVENT_CONTROLLER(click));

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_opacity(sep, 0.1);

    gtk_box_append(GTK_BOX(box), sep);
    gtk_box_append(GTK_BOX(box), row);
}

static int cmp_hist_desc(const void *a, const void *b) {
    const HistoricalEntry *ha = (const HistoricalEntry *)a;
    const HistoricalEntry *hb = (const HistoricalEntry *)b;
    int mc = strcmp(hb->month, ha->month);
    if (mc != 0) return mc;
    if (hb->amount > ha->amount) return 1;
    if (hb->amount < ha->amount) return -1;
    return 0;
}

/* ── Billing card (outstanding) ──────────────────────────────────── */

static GtkWidget *create_billing_card(BillingCardWidgets *w) {
    w->card_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(w->card_revealer),
                                     GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(GTK_REVEALER(w->card_revealer), 400);

    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_add_css_class(card, "card");
    gtk_revealer_set_child(GTK_REVEALER(w->card_revealer), card);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='small' weight='bold' foreground='" ALIBABA_ORANGE "'"
        " letter_spacing='1024'>OUTSTANDING BALANCE</span>");
    gtk_widget_set_halign(title, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(card), title);

    w->total_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(w->total_label),
        "<span size='xx-large' weight='bold' foreground='" TEXT_PRIMARY "'>--</span>\n"
        "<span size='small' foreground='" TEXT_MUTED "'>USD</span>");
    gtk_widget_set_halign(w->total_label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(card), w->total_label);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_opacity(sep, 0.2);
    gtk_box_append(GTK_BOX(card), sep);

    w->breakdown_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_append(GTK_BOX(card), w->breakdown_box);

    w->last_updated_label = gtk_label_new(NULL);
    gtk_widget_set_halign(w->last_updated_label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(card), w->last_updated_label);

    return w->card_revealer;
}

void update_billing_card(BillingCardWidgets *w, BillingData *bd) {
    gtk_spinner_stop(GTK_SPINNER(w->spinner));
    gtk_widget_set_visible(w->spinner, FALSE);

    if (!bd->success) {
        char markup[512];
        snprintf(markup, sizeof(markup),
                 "<span size='large' weight='bold' foreground='" DANGER_RED "'>Error</span>\n"
                 "<span size='small' foreground='" TEXT_MUTED "'>%s</span>", bd->error);
        gtk_label_set_markup(GTK_LABEL(w->total_label), markup);
    } else {
        char markup[320];
        if (bd->total > 0.0) {
            snprintf(markup, sizeof(markup),
                     "<span size='xx-large' weight='bold' foreground='" DANGER_RED "'>$%.2f</span>\n"
                     "<span size='small' foreground='" TEXT_MUTED "'>USD</span>",
                     bd->total);
        } else {
            snprintf(markup, sizeof(markup),
                     "<span size='xx-large' weight='bold' foreground='" SUCCESS_GREEN "'>$0.00</span>\n"
                     "<span size='small' foreground='" TEXT_MUTED "'>USD · All paid</span>");
        }
        gtk_label_set_markup(GTK_LABEL(w->total_label), markup);

        clear_box(w->breakdown_box);
        for (int i = 0; i < bd->count; i++) {
            add_row(w->breakdown_box, bd->items[i].product, bd->items[i].amount,
                    TEXT_SECONDARY, ALIBABA_ACCENT);
        }
    }

    set_timestamp(w->last_updated_label);
    gtk_revealer_set_reveal_child(GTK_REVEALER(w->card_revealer), TRUE);
}

/* ── Services card ───────────────────────────────────────────────── */

static GtkWidget *create_services_card(ServicesCardWidgets *w) {
    w->card_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(w->card_revealer),
                                     GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(GTK_REVEALER(w->card_revealer), 400);

    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_add_css_class(card, "card");
    gtk_revealer_set_child(GTK_REVEALER(w->card_revealer), card);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='small' weight='bold' foreground='" ALIBABA_ORANGE "'"
        " letter_spacing='1024'>MONTHLY SPEND BY SERVICE</span>");
    gtk_widget_set_halign(title, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(card), title);

    w->total_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(w->total_label),
        "<span size='xx-large' weight='bold' foreground='" TEXT_PRIMARY "'>--</span>\n"
        "<span size='small' foreground='" TEXT_MUTED "'>USD · this month</span>");
    gtk_widget_set_halign(w->total_label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(card), w->total_label);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_opacity(sep, 0.2);
    gtk_box_append(GTK_BOX(card), sep);

    w->services_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_append(GTK_BOX(card), w->services_box);

    w->last_updated_label = gtk_label_new(NULL);
    gtk_widget_set_halign(w->last_updated_label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(card), w->last_updated_label);

    return w->card_revealer;
}

void update_services_card(ServicesCardWidgets *w, DetailedBillingData *bd) {
    gtk_spinner_stop(GTK_SPINNER(w->spinner));
    gtk_widget_set_visible(w->spinner, FALSE);

    if (!bd->success) {
        char markup[512];
        snprintf(markup, sizeof(markup),
                 "<span size='large' weight='bold' foreground='" DANGER_RED "'>Error</span>\n"
                 "<span size='small' foreground='" TEXT_MUTED "'>%s</span>", bd->error);
        gtk_label_set_markup(GTK_LABEL(w->total_label), markup);
    } else {
        char markup[320];
        snprintf(markup, sizeof(markup),
                 "<span size='xx-large' weight='bold' foreground='" ALIBABA_ACCENT "'>$%.2f</span>\n"
                 "<span size='small' foreground='" TEXT_MUTED "'>USD · this month</span>",
                 bd->total_spend);
        gtk_label_set_markup(GTK_LABEL(w->total_label), markup);

        clear_box(w->services_box);

        ServiceSummary sorted[MAX_ITEMS];
        memcpy(sorted, bd->services, sizeof(ServiceSummary) * bd->service_count);
        qsort(sorted, bd->service_count, sizeof(ServiceSummary), cmp_service_desc);

        for (int i = 0; i < bd->service_count; i++) {
            double pct = (bd->total_spend > 0) ? (sorted[i].amount / bd->total_spend * 100.0) : 0;
            char label[384];
            snprintf(label, sizeof(label), "%s  <span size='x-small' foreground='" TEXT_MUTED "'>(%.0f%%)</span>",
                     sorted[i].name, pct);
            add_row(w->services_box, label, sorted[i].amount,
                    TEXT_SECONDARY, ALIBABA_ACCENT);
        }
    }

    set_timestamp(w->last_updated_label);
    gtk_revealer_set_reveal_child(GTK_REVEALER(w->card_revealer), TRUE);
}

/* ── Coupons card ────────────────────────────────────────────────── */

static void add_coupon_row(GtkWidget *box, const char *name, double remaining,
                           const char *expiry, double amount) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(row, TRUE);

    GtkWidget *lbl_name = gtk_label_new(NULL);
    char nm[512];
    char expiry_short[32];
    if (expiry && expiry[0]) {
        strncpy(expiry_short, expiry, 10);
        expiry_short[10] = '\0';
    } else {
        strcpy(expiry_short, "N/A");
    }
    snprintf(nm, sizeof(nm),
             "<span size='small' foreground='" TEXT_SECONDARY "'>%s</span>\n"
             "<span size='x-small' foreground='" TEXT_MUTED "'>Expires: %s</span>",
             name, expiry_short);
    gtk_label_set_markup(GTK_LABEL(lbl_name), nm);
    gtk_label_set_wrap(GTK_LABEL(lbl_name), TRUE);
    gtk_widget_set_halign(lbl_name, GTK_ALIGN_START);
    gtk_widget_set_hexpand(lbl_name, TRUE);

    GtkWidget *lbl_amt = gtk_label_new(NULL);
    char am[384];
    if (remaining < amount && remaining > 0.01) {
        snprintf(am, sizeof(am),
                 "<span size='small' weight='bold' foreground='" ALIBABA_ACCENT "'>$%.2f</span>\n"
                 "<span size='x-small' foreground='" TEXT_MUTED "'>of $%.2f</span>",
                 remaining, amount);
    } else {
        snprintf(am, sizeof(am),
                 "<span size='small' weight='bold' foreground='" ALIBABA_ACCENT "'>$%.2f</span>",
                 remaining);
    }
    gtk_label_set_markup(GTK_LABEL(lbl_amt), am);
    gtk_widget_set_halign(lbl_amt, GTK_ALIGN_END);

    gtk_box_append(GTK_BOX(row), lbl_name);
    gtk_box_append(GTK_BOX(row), lbl_amt);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_opacity(sep, 0.1);

    gtk_box_append(GTK_BOX(box), sep);
    gtk_box_append(GTK_BOX(box), row);
}

static GtkWidget *create_coupons_card(CouponCardWidgets *w) {
    w->card_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(w->card_revealer),
                                     GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(GTK_REVEALER(w->card_revealer), 400);

    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_add_css_class(card, "card");
    gtk_revealer_set_child(GTK_REVEALER(w->card_revealer), card);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='small' weight='bold' foreground='" ALIBABA_ORANGE "'"
        " letter_spacing='1024'>AVAILABLE COUPONS</span>");
    gtk_widget_set_halign(title, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(card), title);

    w->total_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(w->total_label),
        "<span size='xx-large' weight='bold' foreground='" TEXT_PRIMARY "'>--</span>\n"
        "<span size='small' foreground='" TEXT_MUTED "'>USD · available balance</span>");
    gtk_widget_set_halign(w->total_label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(card), w->total_label);

    w->cash_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(w->cash_label),
        "<span size='small' foreground='" TEXT_MUTED "'>Cash available: --</span>");
    gtk_widget_set_halign(w->cash_label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(card), w->cash_label);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_opacity(sep, 0.2);
    gtk_box_append(GTK_BOX(card), sep);

    w->coupons_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_append(GTK_BOX(card), w->coupons_box);

    w->last_updated_label = gtk_label_new(NULL);
    gtk_widget_set_halign(w->last_updated_label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(card), w->last_updated_label);

    return w->card_revealer;
}

void update_coupons_card(CouponCardWidgets *w, CouponData *cd) {
    gtk_spinner_stop(GTK_SPINNER(w->spinner));
    gtk_widget_set_visible(w->spinner, FALSE);

    if (!cd->success) {
        char markup[512];
        snprintf(markup, sizeof(markup),
                 "<span size='large' weight='bold' foreground='" DANGER_RED "'>Error</span>\n"
                 "<span size='small' foreground='" TEXT_MUTED "'>%s</span>", cd->error);
        gtk_label_set_markup(GTK_LABEL(w->total_label), markup);
    } else if (cd->count == 0) {
        gtk_label_set_markup(GTK_LABEL(w->total_label),
            "<span size='x-large' weight='bold' foreground='" TEXT_MUTED "'>No coupons</span>\n"
            "<span size='small' foreground='" TEXT_MUTED "'>No active coupons available</span>");
        gtk_label_set_markup(GTK_LABEL(w->cash_label),
            "<span size='small' foreground='" TEXT_MUTED "'>Cash available: $0.00</span>");
        clear_box(w->coupons_box);
    } else {
        char markup[320];
        snprintf(markup, sizeof(markup),
                 "<span size='xx-large' weight='bold' foreground='" SUCCESS_GREEN "'>$%.2f</span>\n"
                 "<span size='small' foreground='" TEXT_MUTED "'>USD · available balance</span>",
                 cd->total_remaining);
        gtk_label_set_markup(GTK_LABEL(w->total_label), markup);

        char cash_markup[320];
        snprintf(cash_markup, sizeof(cash_markup),
                 "<span size='small' foreground='" TEXT_MUTED "'>Cash available: "
                 "<span weight='bold' foreground='" SUCCESS_GREEN "'>$%.2f</span></span>",
                 cd->total_amount);
        gtk_label_set_markup(GTK_LABEL(w->cash_label), cash_markup);

        clear_box(w->coupons_box);
        for (int i = 0; i < cd->count; i++) {
            add_coupon_row(w->coupons_box, cd->items[i].name, cd->items[i].remaining,
                           cd->items[i].expiry, cd->items[i].amount);
        }
    }

    set_timestamp(w->last_updated_label);
    gtk_revealer_set_reveal_child(GTK_REVEALER(w->card_revealer), TRUE);
}

/* ── AI Models card ──────────────────────────────────────────────── */

static void show_model_detail_inline(ModelsCardWidgets *w, const char *model_name);
static void hide_model_detail(ModelsCardWidgets *w);

static void on_back_hover(GtkEventControllerMotion *controller, double x, double y, gpointer user_data) {
    GtkLabel *label = GTK_LABEL(user_data);
    gtk_label_set_markup(label,
        "<span size='small' weight='bold' foreground='" ALIBABA_ORANGE "'>← Back to Models</span>");
}

static void on_back_unhover(GtkEventControllerMotion *controller, gpointer user_data) {
    GtkLabel *label = GTK_LABEL(user_data);
    gtk_label_set_markup(label,
        "<span size='small' weight='bold' foreground='" TEXT_SECONDARY "'>← Back to Models</span>");
}

static void on_back_button_clicked(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    ModelsCardWidgets *w = (ModelsCardWidgets *)user_data;
    hide_model_detail(w);
}

static GtkWidget *create_models_card(ModelsCardWidgets *w, const char *region_name, const char *region_filter) {
    strncpy(w->region_filter, region_filter, sizeof(w->region_filter) - 1);
    w->showing_detail = FALSE;

    w->card_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(w->card_revealer),
                                     GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(GTK_REVEALER(w->card_revealer), 400);

    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_add_css_class(card, "card");
    gtk_revealer_set_child(GTK_REVEALER(w->card_revealer), card);

    char title_markup[256];
    snprintf(title_markup, sizeof(title_markup),
        "<span size='small' weight='bold' foreground='" ALIBABA_ORANGE "'"
        " letter_spacing='1024'>AI MODELS · %s</span>", region_name);
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), title_markup);
    gtk_widget_set_halign(title, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(card), title);

    w->total_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(w->total_label),
        "<span size='xx-large' weight='bold' foreground='" TEXT_PRIMARY "'>--</span>\n"
        "<span size='small' foreground='" TEXT_MUTED "'>USD · LLM inference</span>");
    gtk_widget_set_halign(w->total_label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(card), w->total_label);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_opacity(sep, 0.2);
    gtk_box_append(GTK_BOX(card), sep);

    w->models_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_append(GTK_BOX(card), w->models_box);

    w->last_updated_label = gtk_label_new(NULL);
    gtk_widget_set_halign(w->last_updated_label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(card), w->last_updated_label);

    w->detail_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_visible(w->detail_box, FALSE);
    gtk_box_append(GTK_BOX(card), w->detail_box);

    w->back_button = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(w->back_button),
        "<span size='small' weight='bold' foreground='" TEXT_SECONDARY "'>← Back to Models</span>");
    gtk_widget_set_halign(w->back_button, GTK_ALIGN_START);
    gtk_widget_set_cursor_from_name(w->back_button, "pointer");
    
    GtkGesture *back_click = gtk_gesture_click_new();
    g_signal_connect(back_click, "pressed", G_CALLBACK(on_back_button_clicked), w);
    gtk_widget_add_controller(w->back_button, GTK_EVENT_CONTROLLER(back_click));
    
    GtkEventController *back_motion = gtk_event_controller_motion_new();
    g_signal_connect(back_motion, "enter", G_CALLBACK(on_back_hover), w->back_button);
    g_signal_connect(back_motion, "leave", G_CALLBACK(on_back_unhover), w->back_button);
    gtk_widget_add_controller(w->back_button, back_motion);
    
    gtk_box_append(GTK_BOX(w->detail_box), w->back_button);

    w->detail_title = gtk_label_new(NULL);
    gtk_widget_set_halign(w->detail_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(w->detail_box), w->detail_title);

    w->detail_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_append(GTK_BOX(w->detail_box), w->detail_content);

    return w->card_revealer;
}

static void draw_chart(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    ModelDetailData *detail = (ModelDetailData *)user_data;
    
    if (detail->day_count == 0) return;
    
    // Background
    cairo_set_source_rgb(cr, 0.145, 0.145, 0.145); // CARD_BG
    cairo_paint(cr);
    
    // Find max values for scaling
    double max_cost = 0;
    long max_tokens = 0;
    for (int i = 0; i < detail->day_count; i++) {
        if (detail->daily[i].cost > max_cost) max_cost = detail->daily[i].cost;
        long total = detail->daily[i].input_tokens + detail->daily[i].output_tokens;
        if (total > max_tokens) max_tokens = total;
    }
    
    if (max_cost == 0) max_cost = 1;
    if (max_tokens == 0) max_tokens = 1;
    
    // Chart area
    int margin_left = 50;
    int margin_right = 20;
    int margin_top = 20;
    int margin_bottom = 40;
    int chart_width = width - margin_left - margin_right;
    int chart_height = height - margin_top - margin_bottom;
    
    // Grid lines
    cairo_set_source_rgba(cr, 0.3, 0.3, 0.3, 0.3);
    cairo_set_line_width(cr, 0.5);
    for (int i = 0; i <= 4; i++) {
        int y = margin_top + (chart_height * i / 4);
        cairo_move_to(cr, margin_left, y);
        cairo_line_to(cr, width - margin_right, y);
        cairo_stroke(cr);
        
        // Y-axis labels (cost)
        cairo_set_source_rgb(cr, 0.67, 0.67, 0.67); // TEXT_SECONDARY
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 10);
        char label[32];
        snprintf(label, sizeof(label), "$%.2f", max_cost * (4 - i) / 4);
        cairo_move_to(cr, 5, y + 4);
        cairo_show_text(cr, label);
    }
    
    // Draw bars
    int bar_width = chart_width / detail->day_count;
    int bar_spacing = 2;
    
    for (int i = 0; i < detail->day_count; i++) {
        int x = margin_left + i * bar_width;
        
        // Cost bar (orange)
        double cost_height = (detail->daily[i].cost / max_cost) * chart_height;
        cairo_set_source_rgb(cr, 1.0, 0.416, 0.0); // ALIBABA_ORANGE
        cairo_rectangle(cr, x + bar_spacing, margin_top + chart_height - cost_height,
                       bar_width - bar_spacing * 2, cost_height);
        cairo_fill(cr);
        
        // Token bar (blue, overlaid)
        long total_tokens = detail->daily[i].input_tokens + detail->daily[i].output_tokens;
        double token_height = ((double)total_tokens / max_tokens) * chart_height * 0.3; // 30% of chart height
        cairo_set_source_rgba(cr, 0.2, 0.6, 1.0, 0.6); // Semi-transparent blue
        cairo_rectangle(cr, x + bar_spacing, margin_top + chart_height - token_height,
                       bar_width - bar_spacing * 2, token_height);
        cairo_fill(cr);
        
        // X-axis labels (every 5th day)
        if (i % 5 == 0 || i == detail->day_count - 1) {
            cairo_set_source_rgb(cr, 0.67, 0.67, 0.67);
            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, 9);
            char day_label[16];
            snprintf(day_label, sizeof(day_label), "%d", i + 1);
            cairo_move_to(cr, x + bar_width / 2 - 5, height - margin_bottom + 15);
            cairo_show_text(cr, day_label);
        }
    }
    
    // Legend
    cairo_set_source_rgb(cr, 1.0, 0.416, 0.0);
    cairo_rectangle(cr, width - 150, 10, 12, 12);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.67, 0.67, 0.67);
    cairo_set_font_size(cr, 10);
    cairo_move_to(cr, width - 130, 20);
    cairo_show_text(cr, "Cost ($)");
    
    cairo_set_source_rgba(cr, 0.2, 0.6, 1.0, 0.6);
    cairo_rectangle(cr, width - 150, 28, 12, 12);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.67, 0.67, 0.67);
    cairo_move_to(cr, width - 130, 38);
    cairo_show_text(cr, "Tokens");
}


static void show_model_detail_inline(ModelsCardWidgets *w, const char *model_name) {
    w->showing_detail = TRUE;
    gtk_widget_set_visible(w->models_box, FALSE);
    gtk_widget_set_visible(w->last_updated_label, FALSE);
    gtk_widget_set_visible(w->detail_box, TRUE);

    char title_markup[512];
    snprintf(title_markup, sizeof(title_markup),
        "<span size='large' weight='bold' foreground='" ALIBABA_ACCENT "'>%s</span>\n"
        "<span size='small' foreground='" TEXT_MUTED "'>%s · Billing Breakdown</span>",
        model_name, w->region_filter);
    gtk_label_set_markup(GTK_LABEL(w->detail_title), title_markup);

    clear_box(w->detail_content);

    GtkWidget *loading = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(loading),
        "<span size='medium' foreground='" TEXT_MUTED "'>Loading...</span>");
    gtk_box_append(GTK_BOX(w->detail_content), loading);

    ModelDetailData detail = fetch_model_detail(model_name, w->region_filter);
    w->detail_data = detail;  // Copy to persistent storage

    clear_box(w->detail_content);

    if (!w->detail_data.success || w->detail_data.day_count == 0) {
        GtkWidget *error = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(error),
            "<span size='medium' foreground='" DANGER_RED "'>No billing data available</span>");
        gtk_box_append(GTK_BOX(w->detail_content), error);
        return;
    }

    // Total cost
    char total_markup[256];
    snprintf(total_markup, sizeof(total_markup),
        "<span size='x-large' weight='bold' foreground='" ALIBABA_ACCENT "'>$%.2f</span>\n"
        "<span size='small' foreground='" TEXT_MUTED "'>Total this month</span>",
        w->detail_data.total_cost);
    GtkWidget *total_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(total_label), total_markup);
    gtk_widget_set_halign(total_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(w->detail_content), total_label);

    // Token counts
    char token_markup[512];
    snprintf(token_markup, sizeof(token_markup),
        "<span size='medium' foreground='" TEXT_SECONDARY "'>"
        "Input: <b>%ld</b> tokens · Output: <b>%ld</b> tokens · Total: <b>%ld</b> tokens</span>",
        w->detail_data.total_input_tokens, w->detail_data.total_output_tokens,
        w->detail_data.total_input_tokens + w->detail_data.total_output_tokens);
    GtkWidget *token_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(token_label), token_markup);
    gtk_widget_set_halign(token_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(w->detail_content), token_label);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_opacity(sep, 0.2);
    gtk_box_append(GTK_BOX(w->detail_content), sep);

    // Chart
    GtkWidget *chart_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(chart_label),
        "<span size='small' weight='bold' foreground='" ALIBABA_ORANGE "'"
        " letter_spacing='1024'>USAGE CHART</span>");
    gtk_widget_set_halign(chart_label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(w->detail_content), chart_label);

    GtkWidget *chart_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(chart_area, 600, 200);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(chart_area), draw_chart, &w->detail_data, NULL);
    gtk_box_append(GTK_BOX(w->detail_content), chart_area);

    GtkWidget *sep2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_opacity(sep2, 0.2);
    gtk_box_append(GTK_BOX(w->detail_content), sep2);

    // Daily breakdown header
    GtkWidget *daily_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(daily_header),
        "<span size='small' weight='bold' foreground='" ALIBABA_ORANGE "'"
        " letter_spacing='1024'>DAILY BREAKDOWN</span>");
    gtk_widget_set_halign(daily_header, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(w->detail_content), daily_header);

    for (int i = 0; i < w->detail_data.day_count; i++) {
        // Create a row with cost and tokens
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_hexpand(row, TRUE);

        char row_markup[512];
        snprintf(row_markup, sizeof(row_markup),
            "<span size='small' foreground='" TEXT_SECONDARY "'>%s</span>",
            w->detail_data.daily[i].date);
        GtkWidget *name_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(name_label), row_markup);
        gtk_widget_set_halign(name_label, GTK_ALIGN_START);
        gtk_widget_set_hexpand(name_label, TRUE);
        gtk_box_append(GTK_BOX(row), name_label);

        char amount_markup[256];
        snprintf(amount_markup, sizeof(amount_markup),
            "<span size='small' weight='bold' foreground='" ALIBABA_ACCENT "'>$%.2f</span>"
            "<span size='x-small' foreground='" TEXT_MUTED "'>  ·  %ld in / %ld out</span>",
            w->detail_data.daily[i].cost, w->detail_data.daily[i].input_tokens, w->detail_data.daily[i].output_tokens);
        GtkWidget *amount_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(amount_label), amount_markup);
        gtk_widget_set_halign(amount_label, GTK_ALIGN_END);
        gtk_box_append(GTK_BOX(row), amount_label);

        gtk_box_append(GTK_BOX(w->detail_content), row);
    }
}

static void hide_model_detail(ModelsCardWidgets *w) {
    w->showing_detail = FALSE;
    gtk_widget_set_visible(w->models_box, TRUE);
    gtk_widget_set_visible(w->last_updated_label, TRUE);
    gtk_widget_set_visible(w->detail_box, FALSE);
    clear_box(w->detail_content);
}

void update_models_card(ModelsCardWidgets *w, DetailedBillingData *bd) {
    gtk_spinner_stop(GTK_SPINNER(w->spinner));
    gtk_widget_set_visible(w->spinner, FALSE);

    if (!bd->success) {
        char markup[512];
        snprintf(markup, sizeof(markup),
                 "<span size='large' weight='bold' foreground='" DANGER_RED "'>Error</span>\n"
                 "<span size='small' foreground='" TEXT_MUTED "'>%s</span>", bd->error);
        gtk_label_set_markup(GTK_LABEL(w->total_label), markup);
        return;
    }

    ModelSummary filtered[MAX_ITEMS];
    int filtered_count = 0;
    double model_total = 0;

    for (int i = 0; i < bd->model_count; i++) {
        if (strstr(bd->models[i].region, w->region_filter) != NULL) {
            filtered[filtered_count] = bd->models[i];
            model_total += filtered[filtered_count].amount;
            filtered_count++;
        }
    }

    char markup[320];
    if (filtered_count > 0) {
        snprintf(markup, sizeof(markup),
                 "<span size='xx-large' weight='bold' foreground='" ALIBABA_ACCENT "'>$%.2f</span>\n"
                 "<span size='small' foreground='" TEXT_MUTED "'>USD · %d model%s</span>",
                 model_total, filtered_count, filtered_count == 1 ? "" : "s");
    } else {
        snprintf(markup, sizeof(markup),
                 "<span size='xx-large' weight='bold' foreground='" TEXT_MUTED "'>$0.00</span>\n"
                 "<span size='small' foreground='" TEXT_MUTED "'>No models in this region</span>");
    }
    gtk_label_set_markup(GTK_LABEL(w->total_label), markup);

    clear_box(w->models_box);

    qsort(filtered, filtered_count, sizeof(ModelSummary), cmp_model_desc);

    for (int i = 0; i < filtered_count; i++) {
        double pct = (model_total > 0) ? (filtered[i].amount / model_total * 100.0) : 0;
        add_clickable_model_row(w->models_box, w, filtered[i].model, filtered[i].amount,
                                pct, w->region_filter);
    }

    set_timestamp(w->last_updated_label);
    gtk_revealer_set_reveal_child(GTK_REVEALER(w->card_revealer), TRUE);
}

/* ── Historical card ─────────────────────────────────────────────── */

static GtkWidget *create_historical_card(HistoricalCardWidgets *w) {
    w->card_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(w->card_revealer),
                                     GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(GTK_REVEALER(w->card_revealer), 400);

    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_add_css_class(card, "card");
    gtk_revealer_set_child(GTK_REVEALER(w->card_revealer), card);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='small' weight='bold' foreground='" ALIBABA_ORANGE "'"
        " letter_spacing='1024'>HISTORICAL SPEND (6 MONTHS)</span>");
    gtk_widget_set_halign(title, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(card), title);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_opacity(sep, 0.2);
    gtk_box_append(GTK_BOX(card), sep);

    w->history_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_append(GTK_BOX(card), w->history_box);

    w->last_updated_label = gtk_label_new(NULL);
    gtk_widget_set_halign(w->last_updated_label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(card), w->last_updated_label);

    return w->card_revealer;
}

void update_historical_card(HistoricalCardWidgets *w, HistoricalBillingData *bd) {
    gtk_spinner_stop(GTK_SPINNER(w->spinner));
    gtk_widget_set_visible(w->spinner, FALSE);

    if (!bd->success) {
        clear_box(w->history_box);
        GtkWidget *err = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(err),
            "<span size='large' weight='bold' foreground='" DANGER_RED "'>Error loading history</span>");
        gtk_box_append(GTK_BOX(w->history_box), err);
        return;
    }

    clear_box(w->history_box);

    char current_month[16] = "";
    double month_total = 0;
    gboolean first_month = TRUE;

    HistoricalEntry sorted[MAX_ITEMS * 6];
    memcpy(sorted, bd->entries, sizeof(HistoricalEntry) * bd->count);
    qsort(sorted, bd->count, sizeof(HistoricalEntry), cmp_hist_desc);

    for (int i = 0; i < bd->count; i++) {
        if (strcmp(sorted[i].month, current_month) != 0) {
            if (!first_month && month_total > 0) {
                char total_label[256];
                snprintf(total_label, sizeof(total_label),
                    "<span size='small' weight='bold' foreground='" ALIBABA_ORANGE "'>Month Total</span>");
                add_row(w->history_box, total_label, month_total,
                        ALIBABA_ORANGE, ALIBABA_ORANGE);

                GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
                gtk_box_append(GTK_BOX(w->history_box), spacer);
            }

            strncpy(current_month, sorted[i].month, sizeof(current_month) - 1);
            month_total = 0;
            first_month = FALSE;

            GtkWidget *month_label = gtk_label_new(NULL);
            char month_markup[128];
            snprintf(month_markup, sizeof(month_markup),
                "<span size='medium' weight='bold' foreground='" TEXT_PRIMARY "'>%s</span>",
                sorted[i].month);
            gtk_label_set_markup(GTK_LABEL(month_label), month_markup);
            gtk_widget_set_halign(month_label, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(w->history_box), month_label);
        }

        month_total += sorted[i].amount;
        add_row(w->history_box, sorted[i].service, sorted[i].amount,
                TEXT_SECONDARY, ALIBABA_ACCENT);
    }

    if (!first_month && month_total > 0) {
        char total_label[256];
        snprintf(total_label, sizeof(total_label),
            "<span size='small' weight='bold' foreground='" ALIBABA_ORANGE "'>Month Total</span>");
        add_row(w->history_box, total_label, month_total,
                ALIBABA_ORANGE, ALIBABA_ORANGE);
    }

    set_timestamp(w->last_updated_label);
    gtk_revealer_set_reveal_child(GTK_REVEALER(w->card_revealer), TRUE);
}

/* ── Model Detail Window ─────────────────────────────────────────── */

static void on_model_row_clicked(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    ModelClickData *click_data = (ModelClickData *)user_data;
    show_model_detail_inline(click_data->widgets, click_data->model);
}

/* ── Timer callback ──────────────────────────────────────────────── */

static gboolean on_timer(gpointer user_data) {
    (void)user_data;
    start_fetch(&billing_card);
    start_coupons_fetch(&coupons_card);
    start_detailed_fetch(&services_card, &models_beijing_card, &models_singapore_card);
    start_historical_fetch(&historical_card);
    return G_SOURCE_CONTINUE;
}

/* ── Settings dialog ─────────────────────────────────────────────── */

typedef struct {
    GtkWidget *id_entry;
    GtkWidget *secret_entry;
    GtkWidget *dialog;
} SettingsData;

static GtkWidget *settings_dialog = NULL;

static void on_settings_dialog_destroyed(GtkWidget *widget, gpointer user_data) {
    settings_dialog = NULL;
}

static void on_save_settings(GtkButton *button, gpointer user_data) {
    SettingsData *sd = (SettingsData *)user_data;
    const char *key_id = gtk_editable_get_text(GTK_EDITABLE(sd->id_entry));
    const char *key_secret = gtk_editable_get_text(GTK_EDITABLE(sd->secret_entry));
    save_credentials(key_id, key_secret);
    start_fetch(&billing_card);
    start_detailed_fetch(&services_card, &models_beijing_card, &models_singapore_card);
    start_historical_fetch(&historical_card);
    gtk_window_destroy(GTK_WINDOW(sd->dialog));
}

static void on_settings_clicked(GtkButton *button, gpointer user_data) {
    if (settings_dialog) {
        gtk_window_present(GTK_WINDOW(settings_dialog));
        return;
    }

    GtkWidget *parent = GTK_WIDGET(user_data);

    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Settings");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 420, -1);
    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
    gtk_widget_add_css_class(dialog, "settings-window");
    g_signal_connect(dialog, "destroy", G_CALLBACK(on_settings_dialog_destroyed), NULL);
    settings_dialog = dialog;

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(box, 24);
    gtk_widget_set_margin_bottom(box, 24);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);
    gtk_window_set_child(GTK_WINDOW(dialog), box);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='large' weight='bold' foreground='" ALIBABA_ORANGE "'>API Credentials</span>");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), title);

    GtkWidget *hint = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(hint),
        "<span size='x-small' foreground='" TEXT_MUTED "'>Requires AliyunBSSReadOnlyAccess policy</span>");
    gtk_widget_set_halign(hint, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), hint);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_opacity(sep, 0.2);
    gtk_box_append(GTK_BOX(box), sep);

    GtkWidget *id_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(id_label),
        "<span size='small' foreground='" TEXT_SECONDARY "'>Access Key ID</span>");
    gtk_widget_set_halign(id_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), id_label);

    GtkWidget *id_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(id_entry), "LTAI...");
    const char *current_id = get_access_key_id();
    if (strlen(current_id) > 0)
        gtk_editable_set_text(GTK_EDITABLE(id_entry), current_id);
    gtk_box_append(GTK_BOX(box), id_entry);

    GtkWidget *secret_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(secret_label),
        "<span size='small' foreground='" TEXT_SECONDARY "'>Access Key Secret</span>");
    gtk_widget_set_halign(secret_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), secret_label);

    GtkWidget *secret_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(secret_entry), FALSE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(secret_entry), "Enter access key secret");
    const char *current_secret = get_access_key_secret();
    if (strlen(current_secret) > 0)
        gtk_editable_set_text(GTK_EDITABLE(secret_entry), current_secret);
    gtk_box_append(GTK_BOX(box), secret_entry);

    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_box, 8);
    gtk_box_append(GTK_BOX(box), btn_box);

    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(cancel_btn, "cancel-btn");
    g_signal_connect_swapped(cancel_btn, "clicked", G_CALLBACK(gtk_window_destroy), dialog);
    gtk_box_append(GTK_BOX(btn_box), cancel_btn);

    GtkWidget *save_btn = gtk_button_new_with_label("Save & Refresh");
    gtk_widget_add_css_class(save_btn, "save-btn");
    SettingsData *sd = g_malloc(sizeof(SettingsData));
    sd->id_entry = id_entry;
    sd->secret_entry = secret_entry;
    sd->dialog = dialog;
    g_object_set_data_full(G_OBJECT(dialog), "settings-data", sd, g_free);
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_settings), sd);
    gtk_box_append(GTK_BOX(btn_box), save_btn);

    gtk_window_present(GTK_WINDOW(dialog));
}

/* ── App builder ─────────────────────────────────────────────────── */

void build_ui(GtkApplication *app) {
    apply_css();

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Alibaba Cloud Spend");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 700);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), main_box);

    GtkWidget *header = gtk_center_box_new();
    gtk_widget_set_margin_top(header, 16);
    gtk_widget_set_margin_bottom(header, 8);
    gtk_widget_set_margin_start(header, 24);
    gtk_widget_set_margin_end(header, 24);
    gtk_box_append(GTK_BOX(main_box), header);

    GtkWidget *logo = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(logo),
        "<span size='x-large' weight='bold' foreground='" ALIBABA_ACCENT "'>Alibaba Cloud</span>");
    gtk_center_box_set_center_widget(GTK_CENTER_BOX(header), logo);

    GtkWidget *gear_btn = gtk_button_new_with_label("\xe2\x9a\x99");
    gtk_widget_add_css_class(gear_btn, "flat");
    gtk_widget_add_css_class(gear_btn, "gear-btn");
    gtk_center_box_set_end_widget(GTK_CENTER_BOX(header), gear_btn);
    g_signal_connect(gear_btn, "clicked", G_CALLBACK(on_settings_clicked), window);

    GtkWidget *notebook = gtk_notebook_new();
    gtk_widget_set_vexpand(notebook, TRUE);
    gtk_box_append(GTK_BOX(main_box), notebook);

    /* Tab 1: Overview (Outstanding + Coupons + Services) */
    GtkWidget *tab1_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tab1_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *tab1_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_margin_top(tab1_box, 16);
    gtk_widget_set_margin_bottom(tab1_box, 16);
    gtk_widget_set_margin_start(tab1_box, 24);
    gtk_widget_set_margin_end(tab1_box, 24);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tab1_scroll), tab1_box);

    gtk_box_append(GTK_BOX(tab1_box), create_billing_card(&billing_card));
    gtk_box_append(GTK_BOX(tab1_box), create_coupons_card(&coupons_card));
    gtk_box_append(GTK_BOX(tab1_box), create_services_card(&services_card));

    billing_card.spinner = gtk_spinner_new();
    gtk_widget_set_halign(billing_card.spinner, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(billing_card.spinner, 32, 32);
    coupons_card.spinner = gtk_spinner_new();
    gtk_widget_set_halign(coupons_card.spinner, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(coupons_card.spinner, 32, 32);
    services_card.spinner = gtk_spinner_new();
    gtk_widget_set_halign(services_card.spinner, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(services_card.spinner, 32, 32);

    GtkWidget *tab1_label = gtk_label_new("Overview");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab1_scroll, tab1_label);

    /* Tab 2: AI Models - Beijing */
    GtkWidget *tab2_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tab2_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *tab2_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_margin_top(tab2_box, 16);
    gtk_widget_set_margin_bottom(tab2_box, 16);
    gtk_widget_set_margin_start(tab2_box, 24);
    gtk_widget_set_margin_end(tab2_box, 24);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tab2_scroll), tab2_box);

    gtk_box_append(GTK_BOX(tab2_box), create_models_card(&models_beijing_card, "BEIJING", "Beijing"));

    models_beijing_card.spinner = gtk_spinner_new();
    gtk_widget_set_halign(models_beijing_card.spinner, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(models_beijing_card.spinner, 32, 32);

    GtkWidget *tab2_label = gtk_label_new("AI · Beijing");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab2_scroll, tab2_label);

    /* Tab 3: AI Models - Singapore */
    GtkWidget *tab3_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tab3_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *tab3_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_margin_top(tab3_box, 16);
    gtk_widget_set_margin_bottom(tab3_box, 16);
    gtk_widget_set_margin_start(tab3_box, 24);
    gtk_widget_set_margin_end(tab3_box, 24);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tab3_scroll), tab3_box);

    gtk_box_append(GTK_BOX(tab3_box), create_models_card(&models_singapore_card, "SINGAPORE", "Singapore"));

    models_singapore_card.spinner = gtk_spinner_new();
    gtk_widget_set_halign(models_singapore_card.spinner, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(models_singapore_card.spinner, 32, 32);

    GtkWidget *tab3_label = gtk_label_new("AI · Singapore");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab3_scroll, tab3_label);

    /* Tab 4: Historical */
    GtkWidget *tab4_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tab4_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *tab4_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_margin_top(tab4_box, 16);
    gtk_widget_set_margin_bottom(tab4_box, 16);
    gtk_widget_set_margin_start(tab4_box, 24);
    gtk_widget_set_margin_end(tab4_box, 24);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tab4_scroll), tab4_box);

    gtk_box_append(GTK_BOX(tab4_box), create_historical_card(&historical_card));

    historical_card.spinner = gtk_spinner_new();
    gtk_widget_set_halign(historical_card.spinner, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(historical_card.spinner, 32, 32);

    GtkWidget *tab4_label = gtk_label_new("History");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab4_scroll, tab4_label);

    gtk_window_present(GTK_WINDOW(window));

    start_fetch(&billing_card);
    start_coupons_fetch(&coupons_card);
    start_detailed_fetch(&services_card, &models_beijing_card, &models_singapore_card);
    start_historical_fetch(&historical_card);
    g_timeout_add_seconds(UPDATE_INTERVAL_S, on_timer, NULL);
}
