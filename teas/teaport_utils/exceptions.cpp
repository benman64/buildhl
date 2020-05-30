#include "exceptions.hpp"

#include <condition_variable>
#include <csignal>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

#include "SafePrintf.hpp"
namespace {
    struct LastSignal {
        int get_and_set(int newValue) {
            int result = lastSignal;
            lastSignal = newValue;
            return result;
        }

        void set_signal(int newValue) {
            std::unique_lock<std::mutex> lock(mutex);
            lastSignal = newValue;
            condition.notify_all();
        }
        int wait() {
            std::unique_lock<std::mutex> lock(mutex);
            condition.wait(lock, [this]() -> bool {
                return this->lastSignal != 0;
            });
            return get_and_set(0);
        }
#ifdef _WIN32
        void signal_thread() {

        }
#else
        void signal_thread() {
            sigset_t set;
            sigemptyset(&set);
            sigaddset(&set, SIGINT);
            sigaddset(&set, SIGTERM);

            while (true) {
                int signalNumber;
                if (!sigwait(&set, &signalNumber)) {
                    set_signal(signalNumber);
                }
            }
        }
#endif
        volatile std::sig_atomic_t lastSignal = 0;
        std::mutex mutex;
        std::condition_variable condition;
    };
}

static LastSignal gLastSignal;

#ifdef _WIN32
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    if (fdwCtrlType == CTRL_C_EVENT) {
        gLastSignal.get_and_set(SIGINT);
    } else {
        gLastSignal.get_and_set(SIGTERM);
    }
    return TRUE;
}
#endif

void signal_thread() {
    gLastSignal.signal_thread();
}
static void init_signal_handlers() {
    static bool did_init = false;
    if (did_init)
        return;
    did_init = true;
#ifdef _WIN32
    SetConsoleCtrlHandler(CtrlHandler, TRUE);
#else
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);

    sigprocmask(SIG_BLOCK, &set, nullptr);

    std::thread sigThread(signal_thread);
    sigThread.detach();
#endif
}
namespace tea {
    FatalError::~FatalError() {
        //std::abort();
    }

    std::string code_str(int code) {
        std::string result = "signal ";
        result += std::to_string(code);
        return result;
    }

    std::string command_str(int code) {
        std::string result = "command failed with exit status ";
        result += std::to_string(code);
        return result;
    }
    CommandError::CommandError(int code) : std::runtime_error(command_str(code)) {
        mCode = code;
    }
    int CommandError::exit_code() const noexcept {
        return mCode;
    }

    SignalError::SignalError(int code) : std::runtime_error(code_str(code)) {
        mCode = code;
    }

    int SignalError::code() const noexcept {
        return mCode;
    }
    void throw_signal(int sig) {
        if (sig == 0)
            return;
        if (sig == SIGTERM) {
            throw SIGTERMError(sig);
        } else if(sig == SIGINT) {
            throw SIGINTError(sig);
        }
        throw SignalError(sig);
    }
    void throw_signal_ifneeded() {
        init_signal_handlers();
        std::unique_lock<std::mutex> lock(gLastSignal.mutex);
        int sig = gLastSignal.get_and_set(0);
        throw_signal(sig);
    }
    int wait_for_signal() {
        init_signal_handlers();
        return gLastSignal.wait();
    }
}