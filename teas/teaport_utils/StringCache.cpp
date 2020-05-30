#include "StringCache.hpp"

#include <sys/stat.h>

#include "SafePrintf.hpp"
#include "fileutils.hpp"
#include "exceptions.hpp"
#include "Log.hpp"
#include "DirCache.hpp"

namespace tea {


    StringCache::StringCache(Sqlite3Disowned db) {
        mDb = db;
        assert(mDb.is_open());
        KeyValueTable metaTable(mDb);
        std::string last_version = metaTable.get("string_cache_version");
        if (last_version.empty()) {
            mDb.exec("DROP TABLE IF EXISTS string_cache");
        }
        init_db_ifneeded();
        if (last_version != "1") {
            metaTable.set("string_cache_version", "1");
        }
    }

    std::string StringCache::get(const std::string& key) {
        assert(mDb.is_open());
        Sqlite3Statement statement = mDb.prepare("SELECT id, value from string_cache WHERE key = ?");
        statement.bind_text(1, key);
        if(statement.step() == SQLITE_ROW) {
            int64_t row_id = statement.column_int64(0);
            std::string value = statement.column_text(1);
            update_access_time(row_id);
            return value;
        }
        return "";
    }

    StringCacheRow StringCache::get_row(const std::string& key) {
        assert(mDb.is_open());
        Sqlite3Statement statement = mDb.prepare("SELECT id, value, created, last_modified, last_access from string_cache WHERE key = ?");
        statement.bind_text(1, key);
        if(statement.step() == SQLITE_ROW) {
            StringCacheRow row;
            row.row_id = statement.column_int64(0);
            row.key = key;
            row.value = statement.column_text(1);
            row.created = statement.column_int64(2);
            row.last_modified = statement.column_int64(3);
            row.last_access = statement.column_int64(4);
            update_access_time(row.row_id);
            return row;
        }
        return {};
    }

    void StringCache::set(const std::string& key, const std::string& value) {
        assert(mDb.is_open());
        Sqlite3Statement statement = mDb.prepare("UPDATE string_cache SET last_access = strftime('%s','now'), last_modified = strftime('%s','now'), value = ?  WHERE key = ?");
        statement.bind_text(1, value);
        statement.bind_text(2, key);

        if(statement.step() == SQLITE_ROW) {
            return;
        }

        statement = mDb.prepare("INSERT INTO string_cache (key, value, created, last_modified, last_access) VALUES (?, ?, strftime('%s','now'), strftime('%s','now'), strftime('%s','now'))");
        statement.bind_text(1, key);
        statement.bind_text(2, value);
        if(statement.step() != SQLITE_DONE) {
            throw IOError("could not insert dir into database");
        }
    }

    void StringCache::init_db_ifneeded() {
        assert(mDb.is_open());
        mDb.exec(R"(CREATE TABLE IF NOT EXISTS string_cache (
            id INTEGER PRIMARY KEY,
            key TEXT,
            value TEXT,
            created BIGINT,
            last_modified BIGINT,
            last_access BIGINT
        ))");
        mDb.exec(R"(CREATE UNIQUE INDEX IF NOT EXISTS key_string_cache ON string_cache(key))");
    }

    bool StringCache::update_access_time(int64_t row_id) {
        assert(mDb.is_open());
        Sqlite3Statement statement = mDb.prepare("UPDATE string_cache SET last_access = strftime('%s','now') WHERE id = ?");
        statement.bind_int64(1, row_id);
        return statement.step() == SQLITE_DONE;
    }

    bool StringCache::update_access_time(const std::string& key) {
        assert(mDb.is_open());
        Sqlite3Statement statement = mDb.prepare("UPDATE string_cache SET last_access = strftime('%s','now') WHERE key = ?");
        statement.bind_text(1, key);
        return statement.step() == SQLITE_DONE;
    }
}