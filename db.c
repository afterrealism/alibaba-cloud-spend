#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

static sqlite3 *db = NULL;

static char *get_db_path(void) {
    const char *home = g_get_home_dir();
    if (!home) return NULL;
    return g_build_filename(home, ".config", "alibaba-cloud-spend", "billing.db", NULL);
}

int db_init(void) {
    if (db) return 0;
    
    char *db_path = get_db_path();
    if (!db_path) return -1;
    
    // Ensure directory exists
    char *dir = g_path_get_dirname(db_path);
    g_mkdir_with_parents(dir, 0755);
    g_free(dir);
    
    int rc = sqlite3_open(db_path, &db);
    g_free(db_path);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return rc;
    }
    
    // Create table if not exists
    const char *sql = 
        "CREATE TABLE IF NOT EXISTS billing_snapshots ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp INTEGER NOT NULL,"
        "  model TEXT NOT NULL,"
        "  region TEXT NOT NULL,"
        "  cost REAL NOT NULL,"
        "  input_tokens INTEGER NOT NULL,"
        "  output_tokens INTEGER NOT NULL,"
        "  UNIQUE(timestamp, model, region)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_model_region ON billing_snapshots(model, region, timestamp);";
    
    char *err_msg = NULL;
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return rc;
    }
    
    return 0;
}

int db_insert_snapshot(const BillingSnapshot *snapshot) {
    if (!db) return -1;
    
    const char *sql = 
        "INSERT OR REPLACE INTO billing_snapshots "
        "(timestamp, model, region, cost, input_tokens, output_tokens) "
        "VALUES (?, ?, ?, ?, ?, ?)";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return rc;
    
    sqlite3_bind_int64(stmt, 1, snapshot->timestamp);
    sqlite3_bind_text(stmt, 2, snapshot->model, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, snapshot->region, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 4, snapshot->cost);
    sqlite3_bind_int64(stmt, 5, snapshot->input_tokens);
    sqlite3_bind_int64(stmt, 6, snapshot->output_tokens);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? 0 : rc;
}

BillingHistory *db_query_model_history(const char *model, const char *region, int days) {
    if (!db) return NULL;
    
    BillingHistory *history = malloc(sizeof(BillingHistory));
    if (!history) return NULL;
    
    history->capacity = days + 10;
    history->snapshots = malloc(sizeof(BillingSnapshot) * history->capacity);
    if (!history->snapshots) {
        free(history);
        return NULL;
    }
    history->count = 0;
    
    time_t now = time(NULL);
    time_t cutoff = now - (days * 24 * 60 * 60);
    
    const char *sql = 
        "SELECT timestamp, model, region, cost, input_tokens, output_tokens "
        "FROM billing_snapshots "
        "WHERE model = ? AND region = ? AND timestamp >= ? "
        "ORDER BY timestamp ASC";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        free(history->snapshots);
        free(history);
        return NULL;
    }
    
    sqlite3_bind_text(stmt, 1, model, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, region, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, cutoff);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (history->count >= history->capacity) {
            history->capacity *= 2;
            history->snapshots = realloc(history->snapshots, 
                                         sizeof(BillingSnapshot) * history->capacity);
        }
        
        BillingSnapshot *s = &history->snapshots[history->count++];
        s->timestamp = sqlite3_column_int64(stmt, 0);
        strncpy(s->model, (const char *)sqlite3_column_text(stmt, 1), sizeof(s->model) - 1);
        strncpy(s->region, (const char *)sqlite3_column_text(stmt, 2), sizeof(s->region) - 1);
        s->cost = sqlite3_column_double(stmt, 3);
        s->input_tokens = sqlite3_column_int64(stmt, 4);
        s->output_tokens = sqlite3_column_int64(stmt, 5);
    }
    
    sqlite3_finalize(stmt);
    return history;
}

void db_free_history(BillingHistory *history) {
    if (history) {
        free(history->snapshots);
        free(history);
    }
}
