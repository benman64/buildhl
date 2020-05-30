#pragma once

#include <vector>
#include <string>

#include "Sqlite3.hpp"

namespace tea {
    using namespace csd;
    struct StringCacheRow {
        int64_t row_id          = -1;
        std::string key;
        std::string value;
        int64_t created         = 0;
        int64_t last_modified   = 0;
        int64_t last_access     = 0;
    };
    class StringCache {
    public:
        StringCache(){}
        StringCache(Sqlite3Disowned db);
        StringCache(const StringCache&)=delete;
        StringCache& operator=(const StringCache&)=delete;

        StringCache(StringCache&& other) {
            *this = std::move(other);
        }

        StringCache& operator=(StringCache&& other) {
            mDb = other.mDb;
            other.mDb = nullptr;
            return *this;
        }

        /** Like mkdir but doesn't create the directory.

            @param key  The key to use.
            @return     full path to a directory which may exist. Your responsibility
                        to create the directory.
        */
        std::string get(const std::string& key);
        StringCacheRow get_row(const std::string& key);

        void set(const std::string& key, const std::string& value);

        /** update that you have accessed this file in internal database */
        bool update_access_time(const std::string& key);
        /** update that you have accessed this file in internal database */
        bool update_access_time(int64_t row_id);

        /** create tables if necessary */
        void init_db_ifneeded();
    private:
        Sqlite3Disowned mDb;
    };

}