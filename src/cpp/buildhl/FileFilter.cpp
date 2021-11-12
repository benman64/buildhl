#include "FileFilter.hpp"

#include <teaport_utils/stringutils.hpp>
#include <teaport_utils/fileutils.hpp>

#include <string>
#include <iostream>

namespace buildhl {
    bool is_drive(char c) {
        return (c >= 'a' && c <= 'a') || (c >= 'A' && c <= 'Z');
    }
    std::string clean_path(std::string path) {
        for(std::size_t i = 0; i < path.size(); ++i) {
            if(path[i] == '\\')
                path[i] = '/';
        }
#ifdef _WIN32
        if(path.size() == 2) {
            if(is_drive(path[0]) && path[1] == ':')
                path += '/';
        }
#endif
        auto parts = tea::split(path, '/');
        std::vector<std::string> new_parts;
        for (auto& part : parts) {
            if (part == "..") {
                if (!new_parts.empty())
                    new_parts.pop_back();
            } else if (part == "." || part.empty())
                continue;
            else
                new_parts.push_back(part);
        }
        bool start_slash = path[0] == '/';
        path = tea::join("/", new_parts);
        if (start_slash)
            path = '/' + path;
        return path;
    }

    std::string FileFilter::normalize_path(std::string path) {
        path = buildhl::clean_path(tea::absdir(path));
        for (char& ch : path) {
            if (ch == '\\')
                ch = '/';
        }
        if (m_base_dir.empty() || m_always_absolute)
            return path;
        if (!tea::starts_with(path, m_base_dir.c_str())) {
            return path;
        }
        path = path.substr(m_base_dir.size());
        if (path[0] == '/')
            return "." + path;
        return "./" + path;
    }
    std::string FileFilter::find_file(const std::string& path) {
        if (path.size() > 4096)
            return path;
        if (path.size() <= 2)
            return path;
        using std::isspace;
        if (isspace(path[0]) || isspace(path[path.size()-1]))
            return path;
        const char* not_allowed = "@\"\'‘’`*?<>|[];:";
        for (int i = 0; not_allowed[i]; ++i) {
            if (path.find(not_allowed[i]) != std::string::npos)
                return path;
        }
        if (tea::path_exists(path)) {
            return normalize_path(path);
        }
        for (auto& search_path : m_search_paths) {
            std::string test_path = tea::join_path(search_path, path);
            if (tea::path_exists(test_path)) {
                return normalize_path(test_path);
            }
        }

        std::string up_path = path;
        while (tea::starts_with(up_path, "../") || tea::starts_with(up_path, "..\\")) {
            up_path = up_path.substr(3);
            if (tea::path_exists(up_path)) {
                return normalize_path(up_path);
            }
            for (auto& search_path : m_search_paths) {
                std::string test_path = tea::join_path(search_path, up_path);
                if (tea::path_exists(test_path)) {
                    return normalize_path(test_path);
                }
            }
        }
        return path;
    }

    std::string FileFilter::filter_for(const std::string& str, char delimiter) {
        if (str.find(delimiter) == std::string::npos)
            return find_file(str);
        auto parts = tea::split(str, delimiter);
        for (auto& part : parts) {
            part = find_file(part);
        }
        std::string del(&delimiter, 1);
        return tea::join(del, parts);
    }
    std::string FileFilter::filter(const std::string& str) {
        std::string result = str;
        // all of these delimiters were seen by some tool.
        std::vector<char> delimiters = {
            '(', // MSVC
            '\"', // python
            '\'', // ?
            ':', // gcc, clang
            ';' // ?
        };
        for (char delimiter : delimiters)
            result = filter_for(result, delimiter);
        return result;
    }

    void FileFilter::add_search_path(const std::string& str) {
        m_search_paths.push_back(tea::absdir(str));
    }
    void FileFilter::set_base_dir(const std::string& base) {
        m_base_dir = tea::absdir(base);
        m_search_paths.push_back(m_base_dir);
    }

}