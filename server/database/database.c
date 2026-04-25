#include "database.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

// Create the DB file and table if they don't exist:
int db_init() {
    sqlite3 *db;
    char *err = NULL;

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        fprintf(stderr, "[DB] Error opening DB: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    // Create the table to save our sites: 
    const char *sql =
        "CREATE TABLE IF NOT EXISTS sites ("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  site       TEXT    NOT NULL UNIQUE,"
        "  date_added TEXT    DEFAULT (date('now'))"
        ");";

    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "[DB] Error creating table: %s\n", err);
        sqlite3_free(err);
        sqlite3_close(db);
        return 0;
    }

    // Close the DB and print successful message: 
    printf("[DB] Initialized successfully.\n");
    sqlite3_close(db);

    // Preload the blocked sites for testing: 
    db_add_site("my.wsu.edu");
    db_add_site("login.wsu.edu");
    db_add_site("canvas.wsu.edu");

    return 1;
}

// Load all rows into a SiteList struct:
int db_get_all_sites(SiteList *list) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    list->count = 0;

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        fprintf(stderr, "[DB] Error opening DB: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    const char *sql = "SELECT id, site, date_added FROM sites ORDER BY id ASC;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[DB] Prepare error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW && list->count < MAX_SITES) {
        SiteRecord *r = &list->records[list->count];

        r->id = sqlite3_column_int(stmt, 0);

        strncpy(r->site,
                (const char *)sqlite3_column_text(stmt, 1),
                MAX_SITE_LENGTH - 1);
        r->site[MAX_SITE_LENGTH - 1] = '\0';

        strncpy(r->date_added,
                (const char *)sqlite3_column_text(stmt, 2),
                63);
        r->date_added[63] = '\0';

        list->count++;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 1;
}

// INSERT a site and returns 0 if it already exists:
int db_add_site(const char *site) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        fprintf(stderr, "[DB] Error opening DB: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    const char *sql = "INSERT OR IGNORE INTO sites (site) VALUES (?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[DB] Prepare error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_text(stmt, 1, site, -1, SQLITE_STATIC);
    sqlite3_step(stmt);

    int changed = sqlite3_changes(db);

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    // 0 means site already existed:
    return (changed > 0);  
}

// DELETE a site by name and returns 0 if it wasn't found:
int db_remove_site(const char *site) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        fprintf(stderr, "[DB] Error opening DB: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    const char *sql = "DELETE FROM sites WHERE site = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[DB] Prepare error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_text(stmt, 1, site, -1, SQLITE_STATIC);
    sqlite3_step(stmt);

    int changed = sqlite3_changes(db);

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    // 0 means site wasn't in the DB:
    return (changed > 0);
}

// DELETE all sites:
int db_clear_sites() {
    sqlite3 *db;
    char *err = NULL;

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        fprintf(stderr, "[DB] Error opening DB: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    if (sqlite3_exec(db, "DELETE FROM sites;", NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "[DB] Clear error: %s\n", err);
        sqlite3_free(err);
        sqlite3_close(db);
        return 0;
    }

    sqlite3_close(db);
    return 1;
}

// Check if a site exists  and returns 1 if found:
int db_site_exists(const char *site) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int exists = 0;

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        fprintf(stderr, "[DB] Error opening DB: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    const char *sql = "SELECT 1 FROM sites WHERE site = ? LIMIT 1;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[DB] Prepare error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_text(stmt, 1, site, -1, SQLITE_STATIC);
    exists = (sqlite3_step(stmt) == SQLITE_ROW);

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return exists;
}