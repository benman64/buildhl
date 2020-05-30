#include "highlight.hpp"

#include <string>
#include <vector>
#include <iostream>
#include <assert.h>

#include "lexer.hpp"

bool is_varchar(char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= 'a' && ch <= 'z') || ch == '_';
}

std::string lowercase(std::string str) {
    for (char& ch : str) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = ch - 'A' + 'a';
        }
    }
    return str;
}

std::string lowercase(lex::StaticString str) {
    return lowercase(to_string(str));
}
std::vector<lex::Range> tokenize(lex::StaticString line) {
    std::vector<lex::Range> tokens;
    using namespace lex;
    lex::StaticString cursor(line);
    std::vector<StaticString> symbols {"==", ">=", "<=",
        "+=", "-=", "*=", "/=",
        "::",
        "=", "<", ">", "/", "*", "+", "-", ":", "+", ";", "%", "!", "~",
        "[", "{", "}", "]", "?", "(", ")", "^", "@", "@"};

    for(; cursor; cursor.trim_start(1)) {
        // there should not be more tokens then chars in line
        assert(tokens.size() < line.size());
        if (is_varchar(cursor[0])) {
            Range range = line.range_to_char([](char ch) -> bool {
                return !is_varchar(ch);
            }, line.offset_of(cursor));
            tokens.push_back(range);
            cursor.start = range.end + line.start-1;
        } else if (auto sym = cursor.which_of(symbols)) {
            Range range;
            range.start = cursor.start - line.start;
            range.end = range.start + sym.size();
            cursor.trim_start(sym.size()-1);
            tokens.push_back(range);
        } else if (cursor[0] == '\"') {
            Range range = cursor.range_to([] (const StaticString& str, const StaticString& cursor) -> bool {
                return cursor[0] == '\"' && !(str.count_back_backslashes(str.offset_of(cursor)-1) & 1);
            }, 1);
            auto offset = line.offset_of(cursor);
            range.start = offset;
            range.end += offset + 1;
            tokens.push_back(range);
            cursor.start = range.end -1 + line.start;
        } else if (cursor[0] == '\'') {
            Range range;
            range.start = cursor.start - line.start;
            range.end = range.start;
            while (cursor) {
                cursor.trim_start(1);
                if (cursor[0] == '\'') {
                    if (line.count_back_backslashes(cursor.start - line.start - 1) & 1) {
                        continue;
                    }
                    break;
                }
            }
            range.end = cursor.start - line.start+1;
            tokens.push_back(range);
        }
    }
    return tokens;
}


bool is_numbers(const std::string& str) {
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        for (size_t i = 2; i < str.size(); ++i) {
            if (str[i] >= 'A' && str[i] <= 'F')
                continue;
            if (str[i] >= 'a' && str[i] <= 'f')
                continue;
            if (str[i] >= '0' && str[i] <= '0')
                continue;
            if (str[i] == '_')
                continue;
            return false;
        }

        return true;
    }
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] >= '0' && str[i] <= '9')
            continue;
        if (str[i] == '_')
            continue;
        return false;
    }
    return true;
}

template<typename T>
bool contains(const std::vector<T>& container, const T& value) {
    for (auto& v : container) {
        if (v == value)
            return true;
    }
    return false;
}

template<typename T>
bool contains(const T& container, const typename T::value_type& value) {
    return container.find(value) != container.end();
}

template<typename T>
std::string join(const T& container) {
    std::string result;
    for(auto& str : container) {
        result += str;
    }
    return result;
}
std::string color_line(std::string line_in) {
    using namespace lex;
    if (line_in.empty())
        return line_in;
    StaticString line(line_in.c_str());
    auto tokens = tokenize(line);
    std::vector<std::string> error_words = {"error", "ERROR", "FAILED", "failed"};
    std::vector<std::string> warning_words = {"note", "warning", "WARNING"};
    std::vector<std::string> ok_words = {"ok", "building", "linking", "generating", "done"};
    std::vector<std::string> keywords = {"if", "while", "do",
        "bool", "double", "int", "float", "void",
        "goto",
        "then", "from"};
    std::vector<StaticString> symbols {"==", ">=", "<=",
        "+=", "-=", "*=", "/=",
        "::",
        "=", "<", ">", "/", "*", "+", "-", ":", "+", ";", "%", "!", "~",
        "[", "{", "}", "]", "?", "(", ")", "^", "@", "@"};
    struct Attribute {
        bcolors::cstring name = nullptr;
        Range range;
    };

    std::vector<Attribute> attributes;
    bcolors bcolors;
    for (size_t i = 0; i < tokens.size(); ++i) {
        auto lower = lowercase(line.substr(tokens[i]));
        StaticString tstr = line.substr(tokens[i]);
        if (contains(error_words, lower))
            attributes.push_back({bcolors.FAIL, tokens[i]});
        else if (contains(warning_words, lower))
            attributes.push_back({bcolors.WARNING, tokens[i]});
        else if (is_numbers(lower))
            attributes.push_back({bcolors.NUMBER, tokens[i]});
        else if (contains(ok_words, lower))
            attributes.push_back({bcolors.OK, tokens[i]});
        else if (contains(keywords, lower))
            attributes.push_back({bcolors.NUMBER, tokens[i]});
        else if (tstr.which_of(symbols))
            attributes.push_back({bcolors.SYMBOL, tokens[i]});
        else if (tstr[0] == '\'' || tstr[0] == '\"')
            attributes.push_back({bcolors.STRING, tokens[i]});
    }

    std::sort(attributes.begin(), attributes.end(), [](const Attribute& a, const Attribute& b) -> bool {
        return a.range.start < b.range.start;
    });

    auto last = std::unique(attributes.begin(), attributes.end(), [](const Attribute& a, const Attribute& b) -> bool {
        return a.range.end > b.range.end;
    });
    attributes.erase(last, attributes.end());

    std::string newString = "";
    Range lastRange;
    for (size_t i = 0; i < attributes.size(); ++i) {
        auto attr = attributes[i];
        if (attr.range.start > lastRange.end) {
            // catchup
            newString += line.substr(lastRange.end, attr.range.start - lastRange.end).to_string();
        }
        newString += attr.name;
        newString += line.substr(attr.range).to_string();
        newString += bcolors.ENDC;

        lastRange = attr.range;
    }
    if (lastRange.end < (int)line.size())
        newString += line.substr(lastRange.end).to_string();
    return newString;
}


namespace buildhl {
    std::string nice_num(double num) {
        int whole = num;

        if (whole == 0) {
            int fraction = (num - whole)*100;
            std::string fraction_str;
            if (fraction < 10) {
                fraction_str = "0" + std::to_string(fraction);
            } else {
                fraction_str = std::to_string(fraction);
            }
            return "0." + fraction_str;
        }
        if (whole < 15) {
            int fraction = (num - whole)*10;
            return std::to_string(whole) + "." + std::to_string(fraction);
        }

        return std::to_string(whole);
    }
    std::string nice_time(double seconds) {
        bool negative = seconds < 0;
        if (negative) {
            seconds = -seconds;
        }
        std::string prefix = negative? "-" : "";
        int whole = seconds;

        if (whole == 0) {
            int fraction = (seconds - whole)*100;
            std::string fraction_str;
            if (fraction < 10) {
                fraction_str = "0" + std::to_string(fraction);
            } else {
                fraction_str = std::to_string(fraction);
            }
            return prefix + "0." + fraction_str + " s";
        }
        if (whole < 15) {
            int fraction = (seconds - whole)*10;
            return prefix + std::to_string(whole) + "." + std::to_string(fraction) + " s";
        }
        if (whole  < 60) {
            return prefix + std::to_string(whole) + " s";
        }

        double min = seconds/60;
        return prefix + nice_num(min) + " min";
    }
}