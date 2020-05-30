#include "Log.hpp"

#include <cstdio>
#include <cstring>
#include <assert.h>

#include "exceptions.hpp"
#include "SafePrintf.hpp"

static int error_counts[256];
static bool log_mask[256];
static bool needs_init = true;
static int verbose_level = 1;
namespace tea {
    const char* ErrorCode::kind_str() const {
        switch(code[0]) {
        case 'P':
            return "performance";
        case 'E':
            return "error";
        case 'W':
            return "warning";
        case 'V':
            return "verbose";
        case 'I':
            return "info";
        case 'F':
            return "fatal";
        default:
            return "";
        }
    }
    ErrorCode::ErrorCode(const char* str) {
        *this = str;
    }
    ErrorCode& ErrorCode::operator=(const char* str) {
        assert(strlen(str) < 7);
        strcpy(code, str);
        return *this;
    }
    int ErrorCode::code_int() const {
        int result = 0;
        const char* str = code + 1;
        while(*str) {
            result *= 10;
            result += *str - '0';
            ++str;
        }
        return result;
    }
    size_t fwrite_str(const char* message, FILE* fp) {
        return fwrite(message, 1, strlen(message), fp);
    }

    static void init_log() {
        if(!needs_init)
            return;
        needs_init = false;
        for(std::size_t i = 0; i < sizeof(log_mask)/sizeof(log_mask[0]); ++i)
            log_mask[i] = true;
    }
    void log(const LogMessage& message) {
        init_log();
        ++error_counts[message.error_code.kind_int()];
        if(!log_mask[message.error_code.kind_int()])
            return;
        if(message.error_code.is_verbose() && verbose_level < message.error_code.code_int())
            return;
        if(!message.error_code.is_verbose()) {
            fwrite_str(message.error_code.kind_str(),   stdout);
            fwrite_str(" ",                             stdout);
            fwrite_str(message.error_code.c_str(),      stdout);
            fwrite_str(": ",                            stdout);
        }
        fwrite_str(message.message.c_str(),         stdout);
        fwrite_str("\n",                            stdout);
        if(message.error_code.is_fatal())
            throw FatalError(message.message);
    }

    int log_error_count() {
        return error_counts[(int)(unsigned char)'E'];
    }
    void enable_verbose(int level) {
        verbose_level = level;
    }
}