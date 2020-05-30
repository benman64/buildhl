#pragma once

#include <stdexcept>

namespace tea {
    class IOError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };
    class FatalError: public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
        ~FatalError();
    };
    class FileNotFoundError : public IOError {
    public:
        using IOError::IOError;
    };

    class CommandError : public std::runtime_error {
    public:
        CommandError(int code);
        virtual int exit_code() const noexcept;
    private:
        int mCode;
    };

    class SignalError : public std::runtime_error {
    public:
        SignalError(int code);
        virtual int code() const noexcept;
    private:
        int mCode;
    };

    class SIGINTError : public SignalError {
    public:
        using SignalError::SignalError;
    };

    class SIGTERMError : public SignalError {
    public:
        using SignalError::SignalError;
    };


    void throw_signal_ifneeded();
    void throw_signal(int code);
    int wait_for_signal();
}