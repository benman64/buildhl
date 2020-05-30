#pragma once

#include <string>

namespace tea {
    bool git_make_latest(const std::string& dir, const std::string& git_url);
    /** @returns current commit hash of git at dir */
    std::string git_get_commit_hash(const std::string& dir);
}