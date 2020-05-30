#pragma once

#include <functional>
#include <vector>
#include <string>
#include <map>
#include <cstring>

namespace tea {
    std::vector<std::string> split(const std::string &s, char delim);
    std::vector<std::string> split_no_empty(const std::string &s, char delim);
    std::string join(const std::string& del, const std::vector<std::string>& parts);
    bool starts_with(const char* str, const char* with);
    bool starts_with(const std::string& str, const char* with);
    /** ignores locale, lowercase for english */
    std::string to_lower(std::string str);
    /** parses the input for shell like variables ande replaces them.

        @param input    The input string.
        @param vars     a mapping of variable names to their replacements.
        @return A replaced string.
    */
    std::string replace_string_variables(std::string input, const std::function<std::string(const std::string&)>& vars);
    std::string replace_string_variables(std::string input, const std::map<std::string, std::string>& vars);
    std::string replace_env_vars(std::string input);

    inline bool strequal(const char* a, const char* b) {
        return strcmp(a, b) == 0;
    }
}