#include "link_scanner.hpp"

#include <algorithm>
#include <regex>

#include "shell.hpp"
#include "fileutils.hpp"
#include "container_ops.hpp"

namespace tea {
    std::vector<size_t> find_all(const std::string& contents, const char* what) {
        assert(what != nullptr);
        assert(*what);
        size_t next = 0;
        std::vector<size_t> result;
        size_t len = strlen(what);
        while (true) {
            size_t pos = contents.find(what, next);
            if (pos == std::string::npos)
                break;
            result.push_back(pos);
            next = pos+len;

        }
        return result;
    }

    std::string join_url(std::string left, std::string right) {
        if (left[left.size()-1] == '/' && right[0] != '/') {
            return left + right;
        }
        if (left[left.size()-1] != '/' && right[0] == '/') {
            return left + right;
        }
        if (left[left.size()-1] != '/' && right[0] != '/')
            return left + '/' + right;
        return left + right.substr(1);
    }
    std::vector<std::string> scan_links(const std::string& url) {
        auto process = system_capture_checked({"curl", "-f", "-s", "-L", url});
        process.stdout_data.push_back(0);
        std::string contents = reinterpret_cast<const char*>(&process.stdout_data[0]);

        std::vector<std::string> links;
        auto all_a = find_all(contents, "<a ");
        for (auto anchor_pos : all_a) {
            auto href_pos = contents.find("href", anchor_pos);
            if (href_pos == std::string::npos)
                break;
            size_t start_quote = href_pos;
            while (start_quote < contents.size() && contents[start_quote] != '\'' && contents[start_quote] != '"')
                ++start_quote;
            if (start_quote >= contents.size())
                break;
            size_t end_quote = contents.find(contents[start_quote], start_quote+1);
            if (end_quote == std::string::npos)
                continue;
            std::string link = contents.substr(start_quote+1, end_quote - start_quote-1);
            if (link.find('\\') != std::string::npos)
                continue;
            if (link.find('#') != std::string::npos)
                continue;
            links.push_back(link);
        }

        Url url_parts(url);
        for (auto& link : links) {
            Url link_parts(link);
            if (!link_parts.scheme.empty())
                continue;

            if (link[0] == '/') {
                link = join_url(url_parts.scheme + "://" + url_parts.domain, link);
                continue;
            } else {
                link = join_url(url, link);
            }
        }
        sort_list(links);
        links.erase(std::unique(links.begin(), links.end()), links.end());
        return links;
    }


    std::vector<Version> scan_versions(const std::vector<std::string>& list, const std::string& regex_str) {
        std::regex regex(regex_str);
        std::smatch match;
        std::vector<Version> result;
        for (auto& link : list) {
            if (std::regex_match(link, match, regex)) {
                if (match.size() != 2)
                    continue;
                Version version = static_cast<std::string>(match[1]);
                result.push_back(version);
            }
        }
        return result;
    }
}