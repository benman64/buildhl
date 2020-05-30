#pragma once

#include <string>

#include "lexer.hpp"
struct bcolors {
    typedef const char* cstring;
    cstring HEADER      = "\033[95m";
    cstring OKBLUE      = "\033[94m";
    cstring OKGREEN     = "\033[92m";
    cstring OK          = OKGREEN;
    cstring WARNING     = "\033[93m";
    cstring FAIL        = "\033[91m";
    cstring ENDC        = "\033[0m";
    cstring BOLD        = "\033[1m";
    cstring UNDERLINE   = "\033[4m";
    cstring NUMBER      = "\033[94m";
    cstring STRING      = "\u001b[35m";
    cstring SYMBOL     = "\u001b[36;1m";
    cstring CLEAR_LINE  = "\033[2K";
};

std::vector<lex::Range> tokenize(lex::StaticString line);
std::string color_line(std::string line);

namespace buildhl {
    std::string nice_time(double seconds);
}