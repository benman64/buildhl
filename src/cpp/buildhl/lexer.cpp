#include "lexer.hpp"

#include <assert.h>

namespace lex {
    StaticString::size_type StaticString::find(const char* search, int pos) const {
        if (start == last)
            return npos;
        const char* where = strstr(start+pos, search);
        if (where == nullptr)
            return npos;
        if (where >= last)
            return npos;
        return where - start;
    }
    StaticString StaticString::substr(int start, int len) const {
        StaticString str;

        str.start = this->start + start;
        str.last = str.start + len;
        str.last = std::min(str.last, this->last);
        return str;
    }
    bool StaticString::starts_with(const char* str) const {
        assert(str != nullptr);
        assert(*str);
        for (const char& ch : *this) {
            if (ch != *str)
                return false;
            if(!*str)
                return false;
            ++str;
        }
        return !*str;
    }

    bool StaticString::starts_with(StaticString str_in) const {
        assert(str_in.size() > 0);
        if (str_in.size() > size())
            return false;
        const char* str = begin();
        for (const char& ch : str_in) {
            if (ch != *str)
                return false;
            ++str;
        }
        return true;
    }
    int StaticString::compare(const StaticString str) const {
        if (str.start == start && str.last == last)
            return 0;

        if (str.size() == size()) {
            return strncmp(str.start, start, size());
        }

        int min_size = std::min(str.size(), size());
        int cmp = strncmp(str.start, start, min_size);
        if (cmp)
            return cmp;
        return size() < str.size()? -1 : 1;
    }

    std::string StaticString::to_string() const {
        if (size() == 0) {
            return "";
        }
        return std::string(start, last);
    }
}