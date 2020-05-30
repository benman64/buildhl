#include "jsonutils.hpp"

#include <iostream>

namespace tea {
    using nlohmann::json;

    enum State {
        in_comment,
        in_multiline_comment,
        in_qstring,
        in_qqstring,
        global
    };
    struct Token {
        char token = 0;
        int position = -1;

        explicit operator bool() const {
            return position >= 0 && token != 0;
        }
    };
    void clean_json_comments(std::string& str) {
        State state = State::global;
        Token lastToken;
        for (std::size_t i = 0; i < str.size(); ++i) {
            switch(state) {
            case in_comment:
                if (str[i] == '\n') {
                    state = global;
                } else {
                    str[i] = ' ';
                }
                break;
            case in_multiline_comment:
                if (str[i] == '*' && str[i+1] == '/') {
                    str[i] = ' ';
                    str[i+1] = ' ';
                    state = global;
                }
                str[i] = ' ';
                break;
            case in_qstring:
                if (str[i] == '\'') {
                    state = global;
                }
                lastToken = {};
                break;
            case in_qqstring:
                if (str[i] == '\"') {
                    state = global;
                }
                lastToken = {};
                break;
            case global:
                switch (str[i]) {
                case '\"':
                    state = in_qqstring;
                    lastToken = {};
                    break;
                case '\'':
                    state = in_qstring;
                    lastToken = {};
                    break;
                case '/':
                    if (str[i+1] == '/') {
                        state = in_comment;
                        str[i] = ' ';
                    } else if (str[i+1] == '*') {
                        state = in_multiline_comment;
                        str[i] = ' ';
                        str[i+1] = ' ';
                    }
                    break;
                case ']':
                case '}':
                    if (lastToken) {
                        if (lastToken.token == ',') {
                            str[lastToken.position] = ' ';
                        }
                    }
                    break;
                default:
                    if (isspace(str[i]))
                        break;
                    lastToken.token = str[i];
                    lastToken.position = i;
                    break;
                }
                break;
            }
        }
    }
    void merge_json(json& main, const json& more) {
        for (json::const_iterator it = more.begin(); it != more.end(); ++it) {
            if (it.value().is_null()) {
                continue;
            }
            if (main.find(it.key()) == main.end()) {
                main[it.key()] = it.value();
                continue;
            }
            json& value = main[it.key()];
            if (value.is_null()) {
                main[it.key()] = it.value();
                continue;
            }
            if (value.is_array()) {
                if (it.value().is_array()) {
                    for (auto& val : it.value()) {
                        value.push_back(val);
                    }
                } else
                    value.push_back(it.value());
            } else if (value.is_object()) {
                merge_json(value, it.value());
            } else {
                value = it.value();
            }
        }
    }
}