#ifndef UI_H
#define UI_H

#include "types.h"

void apply_css(void);
void update_billing_card(BillingCardWidgets *w, BillingData *bd);
void update_services_card(ServicesCardWidgets *w, DetailedBillingData *bd);
void update_models_card(ModelsCardWidgets *w, DetailedBillingData *bd);
void update_historical_card(HistoricalCardWidgets *w, HistoricalBillingData *bd);
void build_ui(GtkApplication *app);
void update_proxy_card(ProxyCardWidgets *w);

#endif