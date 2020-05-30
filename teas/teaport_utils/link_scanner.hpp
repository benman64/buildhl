#pragma once

#include <vector>
#include <string>

#include "Version.hpp"
namespace tea {
    std::vector<std::string> scan_links(const std::string& url);
    std::vector<Version> scan_versions(const std::vector<std::string>& list, const std::string& regex_str);
}