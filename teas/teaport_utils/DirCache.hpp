#pragma once

#include <vector>
#include <string>

#include "Sqlite3.hpp"

namespace tea {
    using namespace csd;

    class KeyValueTable {
    public:
        KeyValueTable(){};
        KeyValueTable(Sqlite3Disowned db);

        std::string get(const std::string& key);
        KeyValueTable& set(const std::string& key, const std::string& value);

        Sqlite3Disowned db() const {return mDb;}
    private:
        void init_db_if_needed();
        Sqlite3Disowned mDb;
    };

    struct DirCacheDbEntry {
        int64_t id;
        std::string key;
        std::string kind;
        int64_t created;
        int64_t last_access;

        std::string dir;
    };
    class DirCache {
    public:
        DirCache(){}
        DirCache(Sqlite3Disowned db, const std::string& base_dir);
        DirCache(const DirCache&)=delete;
        DirCache& operator=(const DirCache&)=delete;

        DirCache(DirCache&& other) {
            *this = std::move(other);
        }

        DirCache& operator=(DirCache&& other) {
            mDb = other.mDb;
            mBaseDir = other.mBaseDir;
            other.mDb = nullptr;
            other.mBaseDir = "";
            return *this;
        }
        /** Makes a directory and returns it.

            @param key  The key to use
            @return     full path to created directory, which may have existed
                        already.
        */
        std::string mkdir(const std::string& key);
        /** Like mkdir but doesn't create the directory.

            @param key  The key to use.
            @return     full path to a directory which may exist. Your responsibility
                        to create the directory.
        */
        std::string dir_for_key(const std::string& key);

        /** Deletes the directory and removes it from database */
        void rmdir(const std::string& key);
        /** update that you have accessed this file in internal database */
        void update_access_time(const std::string& key);

        /** create tables if necessary */
        void init_db_ifneeded();

        void set_kind(std::string key, std::string kind);

        std::vector<DirCacheDbEntry> dirs_for_kind(const std::string kind);

    private:
        Sqlite3Disowned mDb;
        std::string     mBaseDir;
    };

}