#pragma once
#include <string>

namespace tea {

    /* use this to see log numbers
    find ./src/ -type f | xargs grep log_message | cut -d \" -f 2 - | sort


    */
    inline int ord(char c) { return (int)((unsigned char)c);}

    struct ErrorCode {
        char code[8] = {0};
        ErrorCode()=default;
        ErrorCode(const ErrorCode&)=default;
        ErrorCode(const char*);
        int kind_int() const {return (int)(unsigned char)code[0];}
        const char* c_str() const {
            return code;
        }
        bool is_fatal() const {return code[0] == 'F';}
        bool is_verbose() const {return code[0] == 'V';}
        int code_int() const;
        const char* kind_str() const;
        ErrorCode& operator=(const char* code);
    };
    struct LogMessage {
        ErrorCode   error_code;
        std::string message;
        std::string file;
        int         line            = 0;
    };
    void log(const LogMessage&);
    inline void log_message(const char* code, const std::string& message) {
        LogMessage msg;
        msg.error_code = code;
        msg.message = message;
        log(msg);
    }
    /// @returns total number of errors loged so far
    int log_error_count();
    void enable_verbose(int level);
}

#define logBase(message) log(message, __FILE__, __LINE__);