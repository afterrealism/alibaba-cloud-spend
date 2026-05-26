#ifndef CONFIG_H
#define CONFIG_H

#include <stdlib.h>

static inline const char *get_access_key_id(void) {
    const char *v = getenv("ALIBABA_CLOUD_ACCESS_KEY_ID");
    return v ? v : "";
}

static inline const char *get_access_key_secret(void) {
    const char *v = getenv("ALIBABA_CLOUD_ACCESS_KEY_SECRET");
    return v ? v : "";
}

#define ENDPOINT          "business.ap-southeast-1.aliyuncs.com"
#define API_VERSION       "2017-12-14"
#define UPDATE_INTERVAL_S 60
#define MAX_ITEMS         32

#endif