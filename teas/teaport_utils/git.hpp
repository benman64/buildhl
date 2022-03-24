#pragma once

#include <string>
#include <vector>

namespace tea {
    bool git_make_latest(const std::string& dir, const std::string& git_url);
    /** @returns current commit hash of git at dir */
    std::string git_get_commit_hash(const std::string& dir);

    struct GitLsResult {
        std::string commit_id;
        std::string path;

        explicit operator bool() const {
            return !commit_id.empty() && !path.empty();
        }
    };

    std::vector<GitLsResult> git_ls_remote(const std::string url);
}