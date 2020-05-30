#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace tea {
    void merge_json(nlohmann::json& main, const nlohmann::json& more);
    void clean_json_comments(std::string& str);
}