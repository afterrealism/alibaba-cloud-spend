#ifndef API_H
#define API_H

#include "types.h"

char *url_encode(const char *str);
char *base64_encode(const unsigned char *data, int len);
char *build_signed_url(const char *action, const char **extra_keys, const char **extra_vals, int extra_count);
BillingData fetch_billing(void);
DetailedBillingData fetch_instance_bills(void);
HistoricalBillingData fetch_historical_bills(void);
ModelDetailData fetch_model_detail(const char *model_name, const char *region);
void start_fetch(BillingCardWidgets *w);
void start_detailed_fetch(ServicesCardWidgets *sw, ModelsCardWidgets *mw_beijing, ModelsCardWidgets *mw_singapore);
void start_historical_fetch(HistoricalCardWidgets *hw);

#endif