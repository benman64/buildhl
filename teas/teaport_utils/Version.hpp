#pragma once

#include <string>
#include <stdexcept>

namespace tea {
    struct VersionParseError : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    /** A version like 1.3.8 */
    class StrictVersion {
    public:
        StrictVersion(){}
        StrictVersion(int major_, int minor_, int patch_=0) {
            major = major_;
            minor = minor_;
            patch = patch_;
        }
        StrictVersion(const std::string &str);


        int major = 0;
        int minor = 0;
        int patch = 0;

        int &operator[](int index) {
            return (&major)[index];
        }

        int compare(const StrictVersion &other) const;
        explicit operator bool() const noexcept {
            return major || minor || patch;
        }

        static StrictVersion from_output(const std::string& cmdOutput);
    #define OP(op) bool operator op (const StrictVersion &other) const { \
            return compare(other) op 0; \
        }

        OP(>)
        OP(<)
        OP(<=)
        OP(>=)
        OP(==)
        OP(!=)
    #undef OP
    };
    std::string to_string(const StrictVersion& version);
    class Version {
    public:
        Version(){}
        Version(int major, int minor, int patch);
        Version(const std::string &str);
        Version(const StrictVersion& version) :
            Version(version.major, version.minor, version.patch) {

        }

        StrictVersion strict_version;
        std::string version_str;
        bool is_str = false;

        bool is_strict() const { return !is_str; }
        Version& operator=(const char* str) {
            *this = Version(str);
            return *this;
        }
        Version& operator=(const std::string& str) {
            *this = Version(str);
            return *this;
        }
        explicit operator bool() const noexcept {
            if (is_str)
                return !version_str.empty();
            return static_cast<bool>(strict_version);
        }
    #define OP(op, first_str) bool operator op (const Version &other) const { \
            if(is_str && other.is_str) { \
                return version_str op other.version_str; \
            } \
            if(is_str == other.is_str) { \
                return strict_version op other.strict_version; \
            } \
            return is_str == first_str; \
        }

        OP(>, false)
        OP(<, true)
        OP(<=, true)
        OP(>=, false)
        OP(==, !is_str)
        OP(!=, is_str)
    #undef OP

    #define OP(op) bool operator op (const char* other) const {\
        return *this op Version(other); \
    }

        OP(>) OP(>=)
        OP(<) OP(<=)
        OP(==) OP(!=)

    #undef OP

    #define OP(op) bool operator op (const std::string& other) const {\
        return *this op Version(other); \
    }
        OP(>) OP(>=)
        OP(<) OP(<=)
        OP(==) OP(!=)

    #undef OP

    };
    std::string to_string(const Version &version);
    enum class Match {
        range,
        none,
        all
    };

    class VersionConstraint {
    public:
        VersionConstraint(const std::string &str);
        VersionConstraint(const Version& v);
        VersionConstraint(){};

        Version min_version;
        Version max_version;

        Match match = Match::none;

        bool check(const Version& version) const;

        VersionConstraint limit(VersionConstraint other) const;

        bool operator==(const VersionConstraint &other) const;
        bool operator!=(const VersionConstraint& other) const;

        VersionConstraint& operator=(const char* other) {
            *this = VersionConstraint(other);
            return *this;
        }
    };

    std::string to_string(const VersionConstraint& version);

}