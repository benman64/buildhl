#include "DirCache.hpp"

#include <sys/stat.h>

#include "SafePrintf.hpp"
#include "fileutils.hpp"
#include "exceptions.hpp"
#include "Log.hpp"

namespace tea {

    KeyValueTable::KeyValueTable(Sqlite3Disowned db) {
        mDb = db;
        init_db_if_needed();
    }

    std::string KeyValueTable::get(const std::string& key) {
        Sqlite3Statement statement = mDb.prepare("SELECT value FROM meta_info WHERE key = ?");
        statement.bind_text(1, key);
        if (statement.step() == SQLITE_ROW) {
            std::string value = statement.column_text(0);
            return value;
        }
        return "";
    }

    KeyValueTable& KeyValueTable::set(const std::string& key, const std::string& value) {
        Sqlite3Statement statement = mDb.prepare("UPDATE meta_info set value = ? where key = ?");
        statement.bind_text(1, value);
        statement.bind_text(2, key);
        if(statement.step() == SQLITE_DONE && mDb.changes() == 1) {
            return *this;
        }
        statement = mDb.prepare("INSERT INTO meta_info (key, value) VALUES (?, ?)");
        statement.bind_text(1, key);
        statement.bind_text(2, value);
        if (statement.step() != SQLITE_DONE) {
            log_message("F1008", "could not add value to meta_info table");
        }
        return *this;
    }
    void KeyValueTable::init_db_if_needed() {
        mDb.exec(R"(CREATE TABLE IF NOT EXISTS meta_info (
            id INTEGER PRIMARY KEY,
            key TEXT,
            value TEXT
        ))");
    }



    /*

    */

    DirCache::DirCache(Sqlite3Disowned db, const std::string& base_dir) {
        mDb = db;
        mBaseDir = base_dir;
        bool is_fresh_dir = false;
        if(!is_dir(mBaseDir)) {
            tea::mkdir(mBaseDir);
            is_fresh_dir = true;
        }
        if(!is_dir(mBaseDir)) {
            log_message("F1007", str_format("{} dir doesn't exist & couldn't be created\n", mBaseDir));
        }

        KeyValueTable metaTable(mDb);
        std::string last_version = metaTable.get("cached_dirs_version");
        if (last_version.empty()) {
            if (!is_fresh_dir) {
                csd::print("Deleting cache dir cause new cache format is not compatible");
            }
            csd::print("rm -rf {}", mBaseDir);
            tea::rmdir(mBaseDir);
            mDb.exec("DROP TABLE IF EXISTS cached_dirs");
            tea::mkdir(mBaseDir);
        }
        init_db_ifneeded();
        if (last_version != "2") {
            metaTable.set("cached_dirs_version", "2");
        }
    }
    std::string DirCache::mkdir(const std::string& key) {
        std::string dir = dir_for_key(key);
        if(!dir.empty()) {
            tea::mkdir(dir);
            return dir;
        }
        throw IOError("could not insert dir into database");
    }

    std::string DirCache::dir_for_key(const std::string& key) {
        Sqlite3Statement statement = mDb.prepare("SELECT id from cached_dirs WHERE key = ?");
        statement.bind_text(1, key);
        if(statement.step() == SQLITE_ROW) {
            int dirId = statement.column_int(0);
            std::string dir = mBaseDir + "/" + std::to_string(dirId);
            return dir;
        }

        statement = mDb.prepare("INSERT INTO cached_dirs (key, created, last_access, kind) VALUES (?, strftime('%s','now'), strftime('%s','now'), '')");
        statement.bind_text(1, key);
        if(statement.step() == SQLITE_DONE) {
            int dirId = mDb.last_insert_rowid();
            std::string dir = mBaseDir + "/" + std::to_string(dirId);
            // this means database was deleted so we don't know what's in this
            // directory. In this case just delete and use it for our purpsose.
            if(path_exists(dir)) {
                tea::rmdir(dir);
            }
            return dir;
        }
        throw IOError("could not insert dir into database");
    }
    void DirCache::rmdir(const std::string& key) {
        Sqlite3Statement statement = mDb.prepare("SELECT id from cached_dirs WHERE key = ?");
        statement.bind_text(1, key);
        if(statement.step() == SQLITE_ROW) {
            int dirId = statement.column_int(0);
            std::string dir = mBaseDir + "/" + std::to_string(dirId);

            tea::rmdir(dir);
        }
    }

    void DirCache::init_db_ifneeded() {
        mDb.exec(R"(CREATE TABLE IF NOT EXISTS cached_dirs (
            id INTEGER PRIMARY KEY,
            key TEXT,
            kind TEXT,
            created BIGINT,
            last_access BIGINT
        ))");
    }

    void DirCache::set_kind(std::string key, std::string kind) {
        Sqlite3Statement statement = mDb.prepare("UPDATE cached_dirs set kind = ? WHERE key = ?");
        statement.bind_text(1, kind);
        statement.bind_text(2, key);
        statement.step();
    }

    std::vector<DirCacheDbEntry> DirCache::dirs_for_kind(const std::string kind) {
        Sqlite3Statement statement = mDb.prepare("SELECT id, key, kind, created, last_access from cached_dirs WHERE kind = ?");
        statement.bind_text(1, kind);
        std::vector<DirCacheDbEntry> result;
        while (statement.step() == SQLITE_ROW) {
            DirCacheDbEntry entry;
            entry.id            = statement.column_int(0);
            entry.key           = statement.column_text(1);
            entry.kind          = statement.column_text(2);
            entry.created       = statement.column_int64(3);
            entry.last_access   = statement.column_int64(4);
            entry.dir           = mBaseDir + "/" + std::to_string(entry.id);
            result.push_back(entry);
        }
        return result;
    }
}