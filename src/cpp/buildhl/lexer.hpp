#pragma once

#include <algorithm>
#include <vector>
#include <string>
#include <cstring>

// OMG MACROS
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace lex {
    struct Range {
        int start   = 0;
        int end     = 0;

        void set_length(int len) {end = start + len;}
        int length() const noexcept {return end - start;}
        Range& merge(const Range& other) {
            start = std::min(other.start, start);
            end = std::max(other.end, end);
            return *this;
        }
        bool intersects(int pos) const {
            return start >= pos && pos < end;
        }
        bool intersects(Range other) const {
            if (intersects(other.start) || intersects(other.end))
                return true;
            if (other.intersects(start) || other.intersects(end))
                return true;
            return false;
        }

        Range& offset(int amount) {
            start += amount;
            end += amount;
            return *this;
        }

        explicit operator bool() const { return start == end; }
    };

    // binary compatible with const char*
    struct CString {
        CString(const CString&)=default;
        CString(const char* other) {
            static_assert(sizeof(*this) == sizeof(const char*), "CString wrong size");
            static_assert(sizeof(CString*) == sizeof(const char**), "CString array wrong size");
            str = other;
        }

        const char* c_str() const {
            return str;
        }

        static int cmp(const char* first, const char* second) {
            if (first == second)
                return 0;
            if (first == nullptr)
                return -1;
            if (second == nullptr)
                return 1;
            while(*first && *second) {
                if (*first < *second)
                    return -1;
                if (*first > *second)
                    return 1;
                ++first;
                ++second;
            }
            if (*first < *second)
                return -1;
            if (*first > *second)
                return 1;
            return 0;
        }

        static int ncmp(const char* first, const char* second, int max_size) {
            if (first == second)
                return 0;
            if (first == nullptr)
                return -1;
            if (second == nullptr)
                return 1;
            int count = 0;
            while(*first && *second && count < max_size) {
                if (*first < *second)
                    return -1;
                if (*first > *second)
                    return 1;
                ++first;
                ++second;
                ++count;
            }
            if (count == max_size)
                return 0;
            if (*first < *second)
                return -1;
            if (*first > *second)
                return 1;
            return 0;

        }
        int compare(const CString& other) const {
            return cmp(this->str, other.str);
        }

        explicit operator const char*() {
            return str;
        }
        
        CString& operator =(const CString&)=default;
        CString &operator=(const char* str) {
            this->str = str;
            return *this;
        }
        #define OP(op) bool operator op(const CString& other) const { \
            return cmp(str, other.str) op 0; \
        } \
        bool operator op(const char* other) const { \
            return cmp(str, other) op 0; \
        }

        OP(>) OP(<) OP(>=) OP(<=) OP(==) OP(!=)
        #undef OP
        
        CString& operator +=(int n) {
            str += n;
            return *this;
        }
        CString& operator -=(int n) {
            str -= n;
            return *this;
        }

        CString& operator ++() {
            ++str;
            return *this;
        }
        CString operator ++(int) {
            auto value = str;
            ++str;
            return value;
        }
        CString operator --(int) {
            auto value = str;
            --str;
            return value;
        }

        bool starts_with(const char* other) const {
            if (other == nullptr || *other == 0)
                return true;
            return ncmp(str, other, strlen(other)) == 0;
        }

        std::size_t size() const {
            return strlen(str);
        }

        const char* str;
    };

    struct StaticString {
        static const auto npos = std::string::npos;
        typedef char value_type;
        typedef std::string::size_type size_type;

        StaticString(){}
        StaticString(const char* cstring) {
            start = cstring;
            last = cstring + std::strlen(start);
        }
        StaticString(const char* cstring, Range range) {
            start = cstring + range.start;
            last = cstring + range.end;
        }

        const char* start   = nullptr;
        const char* last    = nullptr;
        size_type find(const char* search, int pos=0) const;
        size_type find(const std::string& str, int pos=0) const {return find(str.c_str(), pos); }

        template<typename Matcher>
        size_type find_char(const Matcher& matcher, int pos=0) const {
            for (size_type i = pos; i < size(); ++i) {
                if (matcher(start[i]))
                    return i;
            }
            return npos;
        }
        template<typename Matcher>
        Range range_to_char(const Matcher& matcher, size_type pos=0) const {
            Range range;
            range.start = pos;
            for (size_type i = pos; i < size(); ++i) {
                if (matcher(start[i])) {
                    range.end = i;
                    return range;
                }
            }
            range.end = size();
            return range;
        }
        template<typename Matcher>
        Range range_to(const Matcher& matcher, size_type pos=0) const {
            Range range;
            range.start = pos;
            StaticString cursor = substr(pos);
            for (size_type i = pos; cursor; ++i, cursor.trim_start(1)) {
                if (matcher(*this, cursor)) {
                    range.end = i;
                    return range;
                }
            }
            range.end = size();
            return range;
        }
        size_type offset_of(const StaticString& other) const {
            return other.start - start;
        }
        StaticString substr(int start, int len=0x7FFFFFFF) const;
        StaticString substr(Range range) const {
            return substr(range.start, range.length());
        }
        StaticString substr(const char* str) const{
            return substr(str - begin());
        }
        bool starts_with(const char* test) const;
        bool starts_with(StaticString str) const;
        int compare(const StaticString str) const;

        size_t size() const { return last - start;}
        bool empty() const { return size() == 0; }
        explicit operator bool() const { return size() != 0; }

        #define OP(op) bool operator op(StaticString& str) const { return compare(str) op 0; }
        OP(==) OP(!=) OP(>) OP(>=) OP(<) OP(<=)
        #undef OP
        #define OP(op) bool operator op(const char* str) const { return compare(str) op 0; }
        OP(==) OP(!=) OP(>) OP(>=) OP(<) OP(<=)
        #undef OP
        #define OP(op) bool operator op(const std::string& str) const { return compare(str.c_str()) op 0; }
        OP(==) OP(!=) OP(>) OP(>=) OP(<) OP(<=)
        #undef OP

        const value_type& operator[](size_t index) const { return start[index]; }
        std::string to_string() const;

        StaticString& trim_start(int count) {
            start += count;
            if (start > last)
                start = last;
            return *this;
        }
        StaticString& trim_end(int count) {
            last -= count;
            if (last < start)
                last = start;
            return *this;
        }
        StaticString& trim_to(const char* str) {
            while (*this) {
                if (*start == *str && starts_with(str))
                    break;
                ++start;
            }
            return *this;
        }
        bool skip_to_if(const char* begins_with, const char* ends_with) {
            if (starts_with(begins_with)) {
                trim_to(ends_with);
                trim_start(strlen(ends_with));
                return true;
            }
            return false;
        }
        int which_of_index(const std::vector<StaticString>& list) const {
            for (size_t i = 0; i < list.size(); ++i) {
                if (starts_with(list[i])) {
                    return i;
                }
            }
            return -1;
        }
        StaticString which_of(const std::vector<StaticString>& list) const {
            for (size_t i = 0; i < list.size(); ++i) {
                if (starts_with(list[i])) {
                    return list[i];
                }
            }
            return {};

        }

        int count_back_backslashes(int index) const {
            const char* where = start+index;
            const char* cur = where;
            while (cur >= start && *cur == '\\') {
                --cur;
            }
            return where - cur;
        }
        int count_back(int index, char ch) const {
            const char* where = start+index;
            const char* cur = where;
            while (cur >= start && *cur == ch) {
                --cur;
            }
            return where - cur;
        }
        const char* begin() const {return start;}
        const char* end()   const {return last;}

        std::string to_upper() const {
            std::string result;
            for (size_t i = 0; i < size(); ++i) {
                if (start[i] >= 'a' && start[i] <= 'z')
                    result += (char) (start[i] - 'a' + 'A');
                else
                    result += start[i];
            }
            return result;
        }
    };

    inline std::string to_string(const StaticString& str) { return str.to_string(); }
}