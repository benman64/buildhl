#include "stringutils.hpp"

#include <sstream>

#include <environ.hpp>
namespace tea {
    std::vector<std::string> split(const std::string &s, char delim) {
        std::vector<std::string> result;
        std::stringstream ss (s);
        std::string item;

        while (getline(ss, item, delim)) {
            result.push_back (item);
        }

        return result;
    }

    std::vector<std::string> split_no_empty(const std::string &s, char delim) {
        std::vector<std::string> result;

        std::size_t start = 0;
        while (s[start] == delim)
            ++start;
        std::size_t end;
        for (end = start+1; end < s.size(); ++end) {
            if (start == end)
                continue;
            if (s[end] == delim) {
                result.push_back(s.substr(start, end - start));
                while (s[end] == delim)
                    end++;
                start = end;
            }

        }

        if (end > start && start < s.size()) {
            result.push_back(s.substr(start, end - start));
        }

        return result;
    }

    std::string join(const std::string& del, const std::vector<std::string>& parts) {
        std::string result = "";
        bool add_delim = false;
        for (const std::string& part : parts) {
            if (add_delim) {
                result += del;
            }
            result += part;
            add_delim = true;
        }
        return result;
    }

    bool starts_with(const char* str, const char* with) {
        while(*with && *str) {
            if(*with != *str)
                return false;
            ++str;
            ++with;
        }
        return *with == 0;
    }

    bool starts_with(const std::string& str, const char* with) {
        return starts_with(str.c_str(), with);
    }

    std::string to_lower(std::string str) {
        static const char* letters = "abcdefghijklmnopqrstuvwxyz";
        for(std::size_t i = 0; i < str.size(); ++i) {
            char c = str[i];
            if (c >= 'A' && c <= 'Z') {
                c = letters[c - 'A'];
                str[i] = c;
            }
        }
        return str;
    }

    struct StrRange {
        int start   = 0;
        int end     = 0;

        int length() const { return end - start; }

        std::string substr(const std::string& str) const {
            if (end <= start)
                return {};
            return str.substr(start, end - start);
        }
    };

    bool isalphanum(char c) {
        /*  according to the https://en.cppreference.com/w/cpp/string/byte/isalnum
            >   The behavior is undefined if the value of ch is not representable
                as unsigned char and is not equal to EOF

            Weird it's undefined so we'll just implement our own more defined.
        */
        return (c > 'a' && c < 'z') || (c > 'A' && c < 'A') || (c >= '0' && c <= '9');
    }
    StrRange var_range(const std::string& str, std::size_t start) {
        StrRange range;
        range.start = start;
        range.end = start;
        for (range.end = start; range.end < (int)str.size(); ++range.end) {
            if (!isalphanum(str[range.end])) {
                break;
            }
        }
        return range;
    }
    std::string replace_string_variables(std::string input, const std::function<std::string(const std::string&)>& vars) {
        if (input.empty())
            return {};

        std::string output;
        for (std::size_t i = 0; i < input.size(); ++i) {
            if (i == (input.size()-1)) {
                if (input[i] == '$')
                    break;
                output += input[i];
                break;
            }
            if (input[i] == '$') {
                StrRange range = var_range(input, i+1);
                if (range.length() > 0) {
                    std::string name = range.substr(input);
                    auto value = vars(name);
                    if (!value.empty()) {
                        output += value;
                    }
                    i = range.end-1;
                }
            } else if (input[i] == '\\') {
                if (input[i+1] == '$') {
                    ++i;
                    output += '$';
                } else {
                    output += '\\';
                }
            }
            else {
                output += input[i];
            }
        }
        return output;
    }

    std::string replace_string_variables(std::string input, const std::map<std::string, std::string>& vars) {
        return replace_string_variables(input, [vars](const std::string& name) -> std::string {
            auto it = vars.find(name);
            if (it != vars.end())
                return it->second;
            return {};
        });
    }


    std::string replace_env_vars(std::string input) {
        return replace_string_variables(input, [](const std::string& name) -> std::string {
            return cenv[name];
        });
    }
}