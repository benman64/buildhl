#pragma once

#include <sqlite3.h>
#include <string>
#include <stdexcept>
#include <assert.h>
#include <map>

#include "SafePrintf.hpp"

namespace csd {
    // Sqlite3 C api is great. I just want a very very light wrapper
    // just for RAII so I never have to remember to destroy anything.
    extern int gSqlite3StatementCount;
    class Sqlite3;
    class Sqlite3Statement {
    public:
        Sqlite3Statement() { mStatement = nullptr; }
        Sqlite3Statement(Sqlite3 &database, const char *sql);
        Sqlite3Statement(sqlite3_stmt *statement) : mStatement(statement){
            if(mStatement != nullptr) {
                ++gSqlite3StatementCount;
            }
        }
        Sqlite3Statement(Sqlite3Statement &&other) {
            mStatement = nullptr;
            *this = std::move(other);
        }
        Sqlite3Statement &operator=(Sqlite3Statement &&other) {
            finalize();
            mStatement = other.mStatement;
            other.mStatement = nullptr;
            return *this;
        }
        Sqlite3Statement(const Sqlite3Statement&)=delete;
        Sqlite3Statement&operator=(const Sqlite3Statement&)=delete;
        ~Sqlite3Statement() { finalize(); }
        void finalize(){
            if(mStatement) {
                int rc = sqlite3_finalize(mStatement);
                assert(rc == SQLITE_OK);
                if(rc != SQLITE_OK){
                    throw std::runtime_error("could not finalize statement");
                }
                --gSqlite3StatementCount;
                mStatement = nullptr;
            }
        }

        void prepare(sqlite3 *db_c, const char *statement);
        void prepare(sqlite3* db_c, const std::string &statement);
    
        int step() { return sqlite3_step(mStatement); }
        int column_int(int i) { return sqlite3_column_int(mStatement, i); }
        /** Does not clear bindings you must do that by calling clear_bindings */
        int reset() { return sqlite3_reset(mStatement);}
        int clear_bindings() { return sqlite3_clear_bindings(mStatement); }

        /**
            @param index index starts at 1.
        */
        int bind_text(int index, const char *str, int nBytes=-1, void(*freeFunc)(void*)=SQLITE_TRANSIENT) {
            assert(index > 0);
            return sqlite3_bind_text(mStatement, index, str, nBytes, freeFunc);
        }
        /**
            @param index index starts at 1.
        */
        int bind_text(int index, const std::string &str, void(*freeFunc)(void*)=SQLITE_TRANSIENT) {
            assert(index > 0);
            return bind_text(index, str.c_str(), str.size(), freeFunc);
        }

        /**
            @param index index starts at 1.
        */
        int bind_zeroblob64(int index, int64_t size) {
            assert(index > 0);
            return sqlite3_bind_zeroblob64(mStatement, index, size);
        }

        /**
            @param index index starts at 1.
        */
        int bind_int(int index, int value) {
            assert(index > 0);
            return sqlite3_bind_int(mStatement, index, value);
        }
        /**
            @param index index starts at 1.
        */
        int bind_int64(int index, sqlite3_int64 value) {
            assert(index > 0);
            return sqlite3_bind_int64(mStatement, index, value);
        }
        /**
            @param index index starts at 1.
        */
        int bind_double(int index, double value) {
            assert(index > 0);
            return sqlite3_bind_double(mStatement, index, value);
        }

        int column_count() {return sqlite3_column_count(mStatement);}
        const char *column_name (int index) {
            return sqlite3_column_name(mStatement, index);
        }
        sqlite3_int64 column_int64(int index) {
            return sqlite3_column_int64(mStatement, index);
        }
        double column_double(int index) {
            return sqlite3_column_double(mStatement, index);
        }
        const char *column_text(int index) {
            return (const char*)sqlite3_column_text(mStatement, index);
        }

        sqlite3_stmt *get() const noexcept {
            return mStatement;
        }

        bool operator==(const Sqlite3Statement& other) {
            return mStatement == other.mStatement;
        }
        bool operator!=(const Sqlite3Statement& other) {
            return mStatement != other.mStatement;
        }
    private:
        sqlite3_stmt *mStatement;
    };

    /** This wraps around the sqlite3 structure which is the main handle to the
        database
    */
    class Sqlite3 {
    public:
        Sqlite3(const char *fileName) noexcept;
        Sqlite3(const std::string& fileName) noexcept : Sqlite3(fileName.c_str()){}
        Sqlite3() noexcept;
        explicit Sqlite3(sqlite3* db) {mDb = db;}
        Sqlite3(const Sqlite3& other)=delete;
        Sqlite3 &operator=(const Sqlite3 &other)=delete;
        Sqlite3(Sqlite3&& other) {
            mDb         = other.mDb;
            other.mDb   = nullptr;
        }
        Sqlite3& operator=(Sqlite3&& other) {
            close();
            mDb         = other.mDb;
            other.mDb   = nullptr;
            return *this;
        }
        ~Sqlite3();

        int open(const char* fileName);
        int open(const std::string &fileName) {
            return open(fileName.c_str());
        }

        void disown() {
            mDb = nullptr;
        }
        /** Closes database if open.

            @return SQLITE_OK if successful see sqlite3_close() for more
                    details.
        */
        int close() noexcept;
        int exec(const char *sql);
        int exec(const std::string &str) { return exec(str.c_str()); }

        /** @return SQLITE_OK on success or error code on failure */
        int prepare_v2(const char *sql, int nByte, sqlite3_stmt** statement, const char **pzTail = nullptr) {
            assert(mDb != nullptr);
            return sqlite3_prepare_v2(mDb, sql, nByte, statement, pzTail);
        }

        Sqlite3Statement prepare(const char *sql) {
            sqlite3_stmt *statement;
            int rc = prepare_v2(sql, -1, &statement);
            if(rc != SQLITE_OK) {
                throw std::runtime_error(str_format("could not prepare statement error({}): {}", rc, sql));
            }
            return statement;
        }

        Sqlite3Statement prepare(const std::string &sql) {
            return prepare(sql.c_str());
        }

        sqlite3_int64 last_insert_rowid() {
            return sqlite3_last_insert_rowid(mDb);
        }
        int changes() { return sqlite3_changes(mDb); }

        bool is_open() const noexcept {
            return mDb != nullptr;
        }

        sqlite3 *get() noexcept { return mDb; }
        sqlite3 *mDb = nullptr;
    };

    class Sqlite3Disowned : public Sqlite3 {
    public:
        Sqlite3Disowned(){}
        Sqlite3Disowned(const Sqlite3Disowned& other) : Sqlite3() {
            mDb = other.mDb;
        }
        Sqlite3Disowned(sqlite3 *db) : Sqlite3(db){}
        Sqlite3Disowned(Sqlite3& other) {
            mDb = other.mDb;
        }
        ~Sqlite3Disowned() { disown(); }

        Sqlite3Disowned& operator=(const Sqlite3Disowned& other) {
            mDb = other.mDb;
            return *this;
        }
    };

    /** For for doing begin/commit transaction */
    class Sqlite3Transaction {
    public:
        Sqlite3Transaction(Sqlite3 &database) : mDatabase(database){
            mInTransaction = false;
            start();
        }
        ~Sqlite3Transaction() {
            commit();
        }

        int start() {
            if(mInTransaction) {
                throw std::runtime_error("Already in a transaction");
            }
            int rc = mDatabase.exec("BEGIN TRANSACTION");
            if(rc == SQLITE_OK) {
                mInTransaction = true;
            } else {
                throw std::runtime_error("could not begin transaction");
            }
            return rc;
        }

        int commit() {
            if(mInTransaction) {
                mInTransaction = false;
                return mDatabase.exec("COMMIT");
            }
            throw std::runtime_error("No transaction to commit");
            return SQLITE_OK;
        }
    private:
        Sqlite3 &mDatabase;
        bool mInTransaction;
    };

    /** Holds a statement and calls reset automatically when destructed.

        reset or finalize must be called to signal you are done working
        with a stetement so sqlite will unlock tables/databases appropriately.
    */
    class StatementHolder {
    public:
        StatementHolder(Sqlite3Statement *statement) {
            mStatement = statement;
        }
        StatementHolder(StatementHolder&& other) {
            mStatement = nullptr;
            *this = std::move(other);
        }
        ~StatementHolder() { reset(); }
        StatementHolder& operator=(StatementHolder&& other) {
            reset();
            mStatement = other.mStatement;
            other.mStatement = nullptr;
            return *this;
        }

        StatementHolder(const StatementHolder&)=delete;
        StatementHolder &operator=(const StatementHolder&)=delete;

        void reset() {
            if(mStatement) {
                mStatement->reset();
                mStatement->clear_bindings();
            }
        }
        Sqlite3Statement *operator->() const noexcept {
            return mStatement;
        }
        Sqlite3Statement &operator*() const noexcept {
            return *mStatement;
        }

        bool operator==(const StatementHolder& other) const noexcept {
            if(mStatement == nullptr || other.mStatement == nullptr) {
                return mStatement == other.mStatement;
            }
            return *mStatement == *other.mStatement;
        }
    private:
        Sqlite3Statement *mStatement;
    };

    /** Caches statements and returns a prepared statement */
    class StatementCache {
    public:
        typedef std::map<std::string, Sqlite3Statement> map;
        StatementCache(Sqlite3 &database) : mDatabase(database.get()) {}
        StatementCache(sqlite3 *db) :       mDatabase(db) {}
        StatementHolder operator[](const std::string &query) {
            map::iterator it = mCache.find(query);
            if(it == mCache.end()) {
                mCache[query] = mDatabase.prepare(query);
                it = mCache.find(query);
            }
            Sqlite3Statement &statement = it->second;
            statement.reset();
            statement.clear_bindings();
            return StatementHolder(&statement);
        }
    private:
        map             mCache;
        Sqlite3Disowned mDatabase;
    };


    template<typename Parent, typename T>
    class StatementIterator {
    public:
        typedef StatementIterator<Parent, T> self_type;
        StatementIterator(){}
        StatementIterator(Sqlite3Statement &&statement) : mStatement(std::move(statement)) {
            ++(*this);
        }

        self_type &operator++() {
            if(mStatement.get() == nullptr) {
                return *this;
            }
            int rc = mStatement.step();
            if(rc == SQLITE_ROW) {
                Parent::readValue(mValue, mStatement);
            } else {
                mStatement.finalize();
            }
            return *this;
        }

        const T *operator->() const noexcept {
            return &mValue;
        }
        const T &operator*() const noexcept { return mValue; }

        bool operator==(const self_type& other) const noexcept {
            return mStatement.get() == other.mStatement.get();
        }

        bool operator!=(const self_type& other) const noexcept {
            return mStatement.get() != other.mStatement.get();
        }

        Sqlite3Statement    mStatement;
        T                   mValue;
    };

    template<typename Parent, typename T>
    class StatementRefIterator {
    public:
        typedef StatementRefIterator<Parent, T> self_type;
        StatementRefIterator() : mStatement(nullptr) {}
        StatementRefIterator(self_type &&other)
            : mStatement(std::move(other.mStatement)), mValue(other.mValue) {

        }
        StatementRefIterator(StatementHolder &&statement) : mStatement(std::move(statement)) {
            ++(*this);
        }

        self_type &operator++() {
            if(mStatement->get() == nullptr) {
                return *this;
            }
            int rc = mStatement->step();
            if(rc == SQLITE_ROW) {
                Parent::readValue(mValue, *mStatement);
            } else {
                mStatement = nullptr;
            }
            return *this;
        }

        const T *operator->() const noexcept {
            return &mValue;
        }
        const T &operator*() const noexcept { return mValue; }

        bool operator==(const self_type& other) const noexcept {
            if(this == &other) {
                return true;
            }
            return mStatement == other.mStatement;
        }

        bool operator!=(const self_type& other) const noexcept {
            return !(*this == other);
        }

        StatementHolder     mStatement;
        T                   mValue;
    };
    template<typename IT>
    class SingleUseIterable {
    public:
        SingleUseIterable(IT &&first) : mFirst(std::move(first)) {}

        SingleUseIterable(IT &&first, IT &&last) :
            mFirst(std::move(first)), mEnd(std::move(last)) {}
        IT begin() {
            return std::move(mFirst);
        }

        IT end() {
            return std::move(mEnd);
        }
    private:
        IT mFirst;
        IT mEnd;
    };

}
