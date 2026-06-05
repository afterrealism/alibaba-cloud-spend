#ifndef DB_H
#define DB_H

#include <sqlite3.h>
#include <time.h>

typedef struct {
    time_t timestamp;
    char model[128];
    char region[64];
    double cost;
    long input_tokens;
    long output_tokens;
} BillingSnapshot;

typedef struct {
    BillingSnapshot *snapshots;
    int count;
    int capacity;
} BillingHistory;

int db_init(void);
int db_insert_snapshot(const BillingSnapshot *snapshot);
BillingHistory *db_query_model_history(const char *model, const char *region, int days);
void db_free_history(BillingHistory *history);

#endif
