#pragma once
#include <string>
#include <array>
#include <cstring>
#include <utility>
#include <iostream>

namespace csd {

    inline std::string to_string(const char *str) {
        if(str == nullptr) {
            return {};
        }
        return str;
    }

    inline std::string to_string(const std::string &str) {return str;}
    inline std::string to_string(std::string &&str) {return str;}
    inline std::string &to_string(std::string &str) {return str;}

    template<typename Array, typename T, typename... Args>
    void to_array(Array &array, int index, T &value, Args... args) {
        using std::to_string;
        array[index] = to_string(value);
        to_array(array, index+1, args...);
    }

    template<typename... Args>
    std::string str_format(const char *format, Args... args) {
        using std::to_string;
        using csd::to_string;
        std::array<std::string, sizeof...(Args)> stringElements = {to_string(std::forward<Args>(args))...};


        std::string output;
        std::size_t nextIndex = 0;
        std::size_t reserveSize = std::strlen(format);
        for(std::string &str : stringElements) {
            reserveSize += str.size();
        }
        output.reserve(reserveSize + 5);
        for(;*format;++format) {
            if(*format == '\\' && format[1] == '{') {
                ++format;
                output += *format;
                continue;
            }
            if(*format == '{') {
                for(;*format;++format) {
                    if(*format == '}')
                        break;
                }
                if(*format != '}') {
                    throw std::runtime_error("invalid format missing '}'");
                }
                if(nextIndex >= stringElements.size()) {
                    throw std::runtime_error("invalid format missing arguments");
                }
                output += stringElements[nextIndex];
                ++nextIndex;
                continue;
            }
            output += *format;
        }
        return output;
    }


    template<typename... Args>
    void stdout_format(const char *format, Args... args) {
        std::cout << str_format(format, std::forward<Args>(args)...);
        std::flush(std::cout);
    }

    template<typename... Args>
    void print(const char *format, Args... args) {
        std::cout << str_format(format, std::forward<Args>(args)...) << '\n';
        std::flush(std::cout);
    }

}
