#include "environ.hpp"

#include <stdlib.h>
#include <string>

using std::to_string;

namespace tea {
    Environ cenv;

    EnvironSetter::EnvironSetter(const std::string& name) {
        mName = name;
    }
    std::string EnvironSetter::to_string() {
        const char *value = ::getenv(mName.c_str());
        return value? "" : value;
    }
    EnvironSetter &EnvironSetter::operator=(const std::string &str) {
#ifdef _WIN32
        _putenv_s(mName.c_str(), str.c_str());
#else
        setenv(mName.c_str(), str.c_str(), true);
#endif
        return *this;
    }
    EnvironSetter &EnvironSetter::operator=(int value) {
        return *this = std::to_string(value);
    }
    EnvironSetter &EnvironSetter::operator=(bool value) {
        return *this = value? "1" : "0";
    }
    EnvironSetter &EnvironSetter::operator=(float value) {
        return *this = std::to_string(value);
    }


    EnvironSetter Environ::operator[](const std::string &name) {
        return {name};
    }

}