#include "git.hpp"

#include <cstring>
#include <cctype>

#include "shell.hpp"
#include "Log.hpp"
#include "SafePrintf.hpp"

namespace tea {
    bool git_make_latest(const std::string& dir, const std::string& git_url) {
        csd::print("> updating {}", git_url);
        int result = tea::system({"git", "-C", dir, "fetch", "--depth=1"});
        if(result) {
            log_message("E1005", "could not update " + git_url + " in " + dir);
        }
        result = tea::system({"git", "-C", dir, "reset", "--hard", "origin/master"});
        if(result) {
            log_message("E1010", "failed to git reset hard: " + dir + " for " + git_url);
        }
        return result == 0;
    }

    std::string git_get_commit_hash(const std::string& dir) {
        CompletedProcess process = tea::system_capture_checked({"git", "-C", dir, "rev-parse", "HEAD"});

        process.stdout_data.push_back(0);
        std::string hash = reinterpret_cast<const char*>(&process.stdout_data[0]);

        // most inneficiant trimming
        while (std::isspace(hash[hash.size()-1])) {
            hash = hash.substr(0, hash.size()-1);
        }
        while (std::isspace(hash[0]))
            hash = hash.substr(1);
        return hash;
    }
}