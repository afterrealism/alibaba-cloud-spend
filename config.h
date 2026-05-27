#ifndef CONFIG_H
#define CONFIG_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#ifndef _WIN32
#include <sys/stat.h>
#endif

static inline const char *get_access_key_id(void) {
    const char *v = getenv("ALIBABA_CLOUD_ACCESS_KEY_ID");
    return v ? v : "";
}

static inline const char *get_access_key_secret(void) {
    const char *v = getenv("ALIBABA_CLOUD_ACCESS_KEY_SECRET");
    return v ? v : "";
}

static inline void load_credentials(void) {
    const char *home = g_get_home_dir();
    if (!home) return;
    char *path = g_build_filename(home, ".config", "alibaba-cloud-spend", "credentials", NULL);
    FILE *f = fopen(path, "r");
    g_free(path);
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strlen(eq + 1) > 0)
            g_setenv(line, eq + 1, FALSE);
    }
    fclose(f);
}

static inline void save_credentials(const char *key_id, const char *key_secret) {
    const char *home = g_get_home_dir();
    if (!home) return;
    char *dir = g_build_filename(home, ".config", "alibaba-cloud-spend", NULL);
    g_mkdir_with_parents(dir, 0700);
    char *path = g_build_filename(dir, "credentials", NULL);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "ALIBABA_CLOUD_ACCESS_KEY_ID=%s\n", key_id);
        fprintf(f, "ALIBABA_CLOUD_ACCESS_KEY_SECRET=%s\n", key_secret);
        fclose(f);
#ifndef _WIN32
        chmod(path, 0600);
#endif
    }
    g_free(dir);
    g_free(path);
    g_setenv("ALIBABA_CLOUD_ACCESS_KEY_ID", key_id, TRUE);
    g_setenv("ALIBABA_CLOUD_ACCESS_KEY_SECRET", key_secret, TRUE);
}

#define ENDPOINT          "business.ap-southeast-1.aliyuncs.com"
#define API_VERSION       "2017-12-14"
#define UPDATE_INTERVAL_S 60
#define MAX_ITEMS         32

#endif