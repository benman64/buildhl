#pragma once

#include <functional>

namespace tea {
    int add_atexit(const std::function<void()>& func);
    bool remove_atexit(int exit_id);

    struct ExitTransaction {
        ExitTransaction(){}
        ExitTransaction(const std::function<void()>& func) {
            mExitId = add_atexit(func);
        }
        ExitTransaction(const ExitTransaction&)=delete;
        ExitTransaction(ExitTransaction&&other) {
            *this = std::move(other);
        }
        ExitTransaction& operator=(const ExitTransaction&r)=delete;
        ExitTransaction& operator=(ExitTransaction&& other) {
            mExitId         = other.mExitId;
            mSuccess        = other.mSuccess;

            other.mExitId   = 0;
            other.mSuccess  = false;
            return *this;
        }
        ~ExitTransaction() {
        }

        
        void success() {
            mSuccess = true;
            remove_atexit(mExitId);
        }
    private:

        int mExitId     = 0;
        bool mSuccess   = false;
    };
}