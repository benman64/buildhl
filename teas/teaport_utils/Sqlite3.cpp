#include "Sqlite3.hpp"

#include <mutex>

namespace csd {
    int gSqlite3StatementCount = 0;
    void errorLogCallback(void *, int iErrCode, const char *zMsg) {
        static std::mutex *mutex = nullptr;
        if(mutex == nullptr) {
            mutex = new std::mutex;
        }
        std::unique_lock<std::mutex> lock(*mutex);
        fprintf(stderr, "sqlite: (%d) %s\n", iErrCode, zMsg);
    }

    static void initSqlite3() {
        static bool didInit = false;
        if(didInit) {
            return;
        }
        didInit = true;
        sqlite3_config(SQLITE_CONFIG_LOG, errorLogCallback, nullptr);
    }

    void Sqlite3Statement::prepare(sqlite3 *db_c, const char *statement) {
        Sqlite3 db(db_c);
        try {
            *this = db.prepare(statement);
        } catch(...) {
            db.disown();
            throw;
        }
        db.disown();
    }
    void Sqlite3Statement::prepare(sqlite3 *db_c, const std::string &statement) {
        Sqlite3 db(db_c);
        try {
            *this = db.prepare(statement);
        } catch(...) {
            db.disown();
            throw;
        }
        db.disown();
    }

    Sqlite3::Sqlite3() noexcept {
        initSqlite3();
        mDb = nullptr;
    }
    Sqlite3::Sqlite3(const char *fileName) noexcept {
        int rc = open(fileName);
        assert(rc == SQLITE_OK);
        if(rc != SQLITE_OK) {
            std::cerr << "could not open database\n";
            std::abort();
        }
    }

    Sqlite3::~Sqlite3() {
        int rc = close();
        assert(rc == SQLITE_OK);
        if(rc != SQLITE_OK) {
            std::cerr << "could not close sqlite database\n";
            std::abort();
        }
    }

    int Sqlite3::open(const char* fileName) {
        assert(mDb == nullptr);
        initSqlite3();

        int result = sqlite3_open(fileName, &mDb);
        return result;
    }

    int Sqlite3::close() noexcept {
        int rc = SQLITE_OK;
        if(mDb) {
            rc = sqlite3_close(mDb);
            assert(rc == SQLITE_OK);
            if(rc == SQLITE_OK) {
                mDb = nullptr;
            }
        }
        return rc;
    }


    int Sqlite3::exec(const char *sql) {
        char *errorMsg = nullptr;
        if(sql == nullptr) {
            return SQLITE_OK;
        }
        if(*sql == 0) {
            return SQLITE_OK;
        }
        assert(mDb != nullptr);
        // documentation is not clear what this returns
        int rc = sqlite3_exec(mDb, sql, nullptr, nullptr, &errorMsg);
        if(rc != SQLITE_OK) {
            throw std::runtime_error(str_format("sqlite error({}): {}", rc, errorMsg));
        }
        if(errorMsg) {
            // Todo: from exception
            sqlite3_free(errorMsg);
        }
        return rc;
    }
}
