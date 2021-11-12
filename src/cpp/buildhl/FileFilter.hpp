#pragma once

#include <vector>
#include <string>

namespace buildhl {
    class FileFilter {
    public:
        FileFilter(){}
        std::string find_file(const std::string& file);
        std::string filter(const std::string& line);

        void add_search_path(const std::string& str);
        void set_base_dir(const std::string& base);
        std::string get_base_Dir() const {return m_base_dir;}

        void set_always_absolute(bool val) { m_always_absolute = val; }
        bool get_always_absolute() const { return m_always_absolute; }

    private:
        std::string normalize_path(std::string path);
        std::string filter_for(const std::string& line, char delimiter);
        std::vector<std::string> m_search_paths;
        std::string m_base_dir;
        bool m_always_absolute = false;
    };
}