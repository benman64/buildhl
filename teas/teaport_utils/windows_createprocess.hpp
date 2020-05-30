#pragma once
#ifdef _WIN32
#include "shell.hpp"

namespace tea {
    CompletedProcess CreateChildProcess(const char* command, const char* args, bool capture_stderr=false);
}
#endif