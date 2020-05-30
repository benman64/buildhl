#include "Version.hpp"

#include <vector>
#include <sstream>
#include <algorithm>

#include "stringutils.hpp"
#include "SafePrintf.hpp"

using std::vector;
using std::string;

namespace tea {
    StrictVersion::StrictVersion(const std::string &str) {
        for(char c : str) {
            if(c >= '0' && c <= '9')
                continue;
            if(c == '.')
                continue;
            throw VersionParseError("invalid character in version");
        }
        vector<string> parts = split(str, '.');
        if(parts.size() > 3) {
            throw VersionParseError("too many parts for StrictVersion");
        }
        if(parts.size() == 0) {
            throw VersionParseError("null string");
        }
        for(auto& str : parts) {
            if (str.size() == 0) {
                throw VersionParseError("empty version");
            }
            for (auto& ch : str) {
                if (!std::isdigit(ch)) {
                    throw VersionParseError("invalid char in version");
                }
            }
        }
        major = std::stoi(parts[0]);
        if(parts.size() >= 2)
            minor = std::stoi(parts[1]);
        if(parts.size() >= 3)
            patch = std::stoi(parts[2]);
    }

    int StrictVersion::compare(const StrictVersion &other) const {
        if(other.major != major)
            return major > other.major? 1 : -1;
        if(other.minor != minor)
            return minor > other.minor? 1 : -1;
        if(other.patch != patch)
            return patch > other.patch? 1 : -1;
        return 0;
    }
    StrictVersion StrictVersion::from_output(const std::string& cmdOutput) {
        std::vector<std::string> lines = split_no_empty(cmdOutput, '\n');

        for (std::string& line : lines) {
            int nests = 0;
            for (char& c : line) {
                if (c == '\r')
                    c = ' ';
                if (c == '(')
                    ++nests;
                if (c == '(') {
                    --nests;
                    c = ' ';
                }
                if (nests > 0)
                    c = ' ';
                if (!std::isdigit(c) && c != '.') {
                    c = ' ';
                }
            }
            std::vector<std::string> parts = split_no_empty(line, ' ');
            for (std::string part : parts) {
                if (part.find('.') == part.npos)
                    continue;
                if (part.find("..") != part.npos)
                    continue;
                if (split_no_empty(part, '.').size() <= 1)
                    continue;
                try {
                    StrictVersion version(part);
                    return version;
                } catch (VersionParseError& error) {
                    continue;
                }
            }
        }
        return {};
    }

    std::string to_string(const StrictVersion& version) {
        std::string result;
        using std::to_string;
        result = to_string(version.major) + "." + to_string(version.minor) + "." + to_string(version.patch);
        return result;
    }
    Version::Version(const std::string &str) {
        try {
            strict_version = StrictVersion(str);
        } catch(VersionParseError &e) {
            is_str = true;
        }
        version_str = str;
    }
    Version::Version(int major, int minor, int patch) {
        strict_version = StrictVersion(major, minor, patch);
        is_str = false;
    }


    std::string to_string(const Version &version) {
        if(version.is_str) {
            return version.version_str;
        }
        return to_string(version.strict_version);
    }

    VersionConstraint::VersionConstraint(const std::string &str) {
        if(str.size() == 0 || str == "*") {
            match = Match::all;
            return;
        }
        match = Match::range;

        for(std::size_t i = 0; i < str.size(); ++i) {
            if(!isdigit(str[i]) && str[i] != '.' && str[i] != '+') {
                min_version = Version(str);
                max_version = min_version;
                match = Match::range;
                return;
            }
        }
        vector<string> parts = split(str, '.');

        if(parts.size() > 3) {
            min_version = Version(str);
            match = Match::range;
            return;
        }
        int size = std::min<int>(parts.size(), 3);
        for(int i = 0; i < size; ++i) {
            if(parts[i].size() == 0 || parts[i][0] == '+') {
                min_version.strict_version[i] = 0;
            } else {
                min_version.strict_version[i] = stoi(parts[i]);
            }
        }
        max_version = min_version;
        // check for plus
        string last = parts[parts.size()-1];
        if(last[last.size()-1] == '+') {
            max_version.strict_version[parts.size()-1] = 0x7FFFFFFF;
        } else {
            ++max_version.strict_version[parts.size()-1];
        }
    }

    VersionConstraint::VersionConstraint(const Version& v) {
        match = Match::range;
        min_version = v;
        max_version = v;
        if (!v.is_str) {
            max_version.strict_version.patch += 1;
        }
    }

    bool VersionConstraint::check(const Version& version) const {
        if(match == Match::none)
            return false;
        if(match == Match::all)
            return true;
        if(min_version.is_str) {
            return version == min_version;
        }
        return version >= min_version && version < max_version;
    }

    VersionConstraint VersionConstraint::limit(VersionConstraint other) const {
        if(match == Match::none || other.match == Match::none)
            return {};
        if(match == Match::all)
            return other;
        if(other.match == Match::all)
            return *this;
        if(other.min_version.is_str || min_version.is_str) {
            if(other.min_version == min_version)
                return *this;
            return {};
        }
        VersionConstraint result;
        result.match = Match::range;
        result.min_version = std::max(min_version, other.min_version);
        result.max_version = std::min(max_version, other.max_version);
        if(result.min_version >= result.max_version)
            return {};
        return result;
    }
    bool VersionConstraint::operator==(const VersionConstraint &other) const {
        if(match == other.match) {
            if(match == Match::all)
                return true;
            if(match == Match::none)
                return true;
            return min_version == other.min_version && max_version == other.max_version;
        }
        return false;
    }

    bool VersionConstraint::operator!=(const VersionConstraint& other) const {
        return !(*this == other);
    }

    std::string to_string(const VersionConstraint& version) {
        if(version.match == Match::all)
            return "+";
        if(version.match == Match::range) {
            if(version.min_version.is_str)
                return to_string(version.min_version);
            if(version.min_version == version.max_version)
                return to_string(version.min_version);
            std::string result = "[";
            result += to_string(version.min_version) + ", " + to_string(version.max_version) + "]";
            return result;
        }
        // matches nothing
        return "none";
    }

}