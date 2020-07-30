#include "fileutils.hpp"

#include <sys/types.h>
#include <sys/stat.h>

#include <cstring>
#include <assert.h>
#include <fstream>
#include <filesystem>
#include <random>

#ifdef _WIN32
#include <windows.h>
#endif


#include "SafePrintf.hpp"

#include "shell.hpp"
#include "Log.hpp"
#include "jsonutils.hpp"
#include "exceptions.hpp"

namespace fs = std::filesystem;

namespace tea {
    CFile::CFile(FILE* fp) {
        mFile = fp;
    }
    CFile::~CFile() {
        if (mFile)
            fclose(mFile);
    }
    int CFile::read(void* buffer, int size) {
        return ::fread(buffer, 1, size, mFile);
    }
    int CFile::write(const void* buffer, int size) {
        return ::fwrite(buffer, 1, size, mFile);
    }

    std::string CFile::read_str(int size) {
        std::string result(size, 0);
        int transfered = read(&result[0], size);
        if (transfered >= 0) {
            result.resize(transfered);
            return result;
        }
        return "";
    }

    void CFile::close() {
        if (mFile) {
            fclose(mFile);
            mFile = nullptr;
        }
    }
    bool mkdir(const std::string& dir, int mode) {
        throw_signal_ifneeded();
        return std::filesystem::create_directory(dir);
    }
    bool mkdir_p(const std::string& dir) {
        throw_signal_ifneeded();
        return std::filesystem::create_directories(dir);
    }
    bool path_exists(const std::string& path) {
        return is_file(path) || is_dir(path);
    }
    bool path_exists(const fs::path& path) {
        return path_exists(path.native());
    }

    std::string absdir(std::string dir, std::string relativeTo) {
        dir = clean_path(dir);
        if(is_absolute_path(dir))
            return dir;
        if(relativeTo.empty())
            relativeTo = getcwd();
        if(!is_absolute_path(relativeTo)) {
            relativeTo = join_path(getcwd(), relativeTo);
        }
        return join_path(relativeTo, dir);
    }

    std::string getcwd() {
        throw_signal_ifneeded();
        return std::filesystem::current_path().string();
    }
    bool chdir(const std::string& path) {
        throw_signal_ifneeded();
        try {
            fs::current_path(path);
        } catch(...) {
            return false;
        }
        return true;
    }
    std::string dirname(std::string path) {
        std::string::size_type pos = path.rfind('/');
        if(pos != std::string::npos) {
            return path.substr(0, pos);
        }
        // for windows
        pos = path.rfind('\\');
        if(pos != std::string::npos) {
            return path.substr(0, pos);
        }
        return "";
    }
    std::string basename(std::string path) {
        std::string::size_type pos = path.rfind('/');
        if(pos == std::string::npos)
            pos = path.rfind('\\');

        if(pos != std::string::npos) {
            return path.substr(pos+1);
        }
        return path;
    }

    bool files_readonly_recurse(const fs::path& path) {
        bool success = true;
        const auto all_write = std::filesystem::perms::owner_write |
            std::filesystem::perms::group_write |
            std::filesystem::perms::others_write;

        if (is_dir(path.string())) {
            // try 3 times
            for (int i = 0; i < 3; ++i) {
                success = true;
                try {
                    for (auto& it : RecursiveDirIt(path)) {
                        if (it.is_directory())
                            continue;
                        std::filesystem::permissions(
                            it.path(),
                            all_write,
                            std::filesystem::perm_options::remove
                        );
                    }
                } catch (...) {
                    success = false;
                }
            }
        } else {
            try {
                std::filesystem::permissions(
                    path,
                    all_write,
                    std::filesystem::perm_options::remove
                );
            } catch (...) {
                success = false;
            }
        }
        return success;
    }

    bool owner_write_recurse(const fs::path& path) {
        bool success = true;

        if (is_dir(path.string())) {
            // try 3 times
            for (int i = 0; i < 3; ++i) {
                success = true;
                try {
                    for (auto& it : RecursiveDirIt(path)) {
                        std::filesystem::permissions(
                            it.path(),
                            std::filesystem::perms::owner_write,
                            std::filesystem::perm_options::add
                        );
                    }
                } catch (...) {
                    success = false;
                }
            }
        } else {
            try {
                std::filesystem::permissions(
                    path,
                    std::filesystem::perms::owner_write,
                    std::filesystem::perm_options::add
                );
            } catch (...) {
                success = false;
            }
        }
        return success;
    }

    bool rmdir(const std::string& path) {
        if(path.empty())
            return false;
        assert(path != "/");
        owner_write_recurse(path);

        bool success = false;
        try {
            std::filesystem::remove_all(path);
            success = true;
        } catch(std::exception& e) {
        }
        return success;
    }

    bool rmdir(const fs::path& path) {
        if(path.empty())
            return false;
        assert(path.string() != "/");

        // add write permissions so we can remove the file on windows
        owner_write_recurse(path);

        bool success = false;
        try {
            std::filesystem::remove_all(path);
            success = true;
        } catch(std::exception& e) {
        }
        return success;
    }

    bool is_dir(const std::string &path) {
        try {
            if (path.empty())
                return false;
            return std::filesystem::is_directory(path);
        } catch (std::filesystem::filesystem_error& e) {
            return false;
        }
    }
    bool is_file(const std::string &path) {
        try {
            if (path.empty())
                return false;
            return std::filesystem::is_regular_file(path);
        } catch (std::filesystem::filesystem_error& e) {
            return false;
        }
    }

    bool is_link(const std::string& path) {
        try {
            if (path.empty())
                return false;
            return std::filesystem::is_symlink(path);
        } catch (std::filesystem::filesystem_error& e) {
            return false;
        }
    }

    bool copy_file(const std::string& src, const std::string& dst) {
        throw_signal_ifneeded();
        if(is_link(src)) {
            try {
                std::filesystem::copy_symlink(src, dst);
            } catch(std::filesystem::filesystem_error& e) {
                return false;
            }
            return true;
        }
        return std::filesystem::copy_file(src, dst,
            std::filesystem::copy_options::overwrite_existing);

    }

    bool copy_dir(std::string src, std::string dst) {
        throw_signal_ifneeded();
        while(src[src.size()-1] == '/')
            src.resize(src.size()-1);
        while(dst[dst.size()-1] == '/')
            dst.resize(dst.size()-1);

        try {
            std::filesystem::copy(src, dst, std::filesystem::copy_options::recursive);
        } catch(std::filesystem::filesystem_error& e) {
            return false;
        }
        return true;
    }
    bool symlink(std::string src, std::string dst) {
        throw_signal_ifneeded();
        while(src[src.size()-1] == '/')
            src.resize(src.size()-1);
        while(dst[dst.size()-1] == '/')
            dst.resize(dst.size()-1);

        try {
            if(is_dir(src))
                std::filesystem::create_directory_symlink(src, dst);
            else
                std::filesystem::create_symlink(src, dst);
        } catch(std::filesystem::filesystem_error& e) {
            return false;
        }
        return true;
    }

    bool isalnum(int x) {
        if(x >= 'a' || x <= 'z' || x >= '0' || x <= '9')
            return true;
        if(x >= 'A' || x <= 'Z')
            return true;
        return false;
    }
    std::string expand_path(std::string path) {
        std::string result;
        std::size_t cursor = 0;
        std::size_t last_copy = 0;
        if(path[0] == '~' && path[1] == '/') {
            path = path.substr(1);
            std::string value = home_dir();
            std::string new_path;
            if(!value.empty()) {
                new_path = value;
                new_path += path;
            }
        }
        for(;cursor < path.size(); ++cursor) {
            if(path[cursor] == '$') {
                if(cursor > last_copy)
                    result += path.substr(last_copy, cursor - last_copy);
                ++cursor;
                last_copy = cursor;
                while(isalnum(path[cursor]) && cursor < path.size())
                    ++cursor;
                std::string key = result.substr(last_copy, cursor - last_copy);
                std::string value = tea::getenv(key);
                if(!value.empty())
                    result += value;
                last_copy = cursor;
                --cursor;
                continue;
            }
        }
        if(last_copy < path.size())
            result += path.substr(last_copy);
        return result;
    }

    bool is_drive(char c) {
        return (c >= 'a' && c <= 'a') || (c >= 'A' && c <= 'Z');
    }
    bool is_absolute_path(const std::string& path) {
        if(path.empty())
            return false;
#ifdef _WIN32
        if(is_drive(path[0]) && path[1] == ':')
            return true;
        return false;
#else
        if(path[0] == '/')
            return true;
        return false;
#endif
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
        while(path.size() >= 2 && path[path.size()-1] == '/' && path[path.size()-2] == '/')
            path.resize(path.size()-1);
        return path;
    }
    using nlohmann::json;

    json parse_json(std::string js_str) {
        json js;
        try {
            clean_json_comments(js_str);
            js = json::parse(js_str);
        } catch(std::exception& e) {
            log_message("E1023", csd::str_format("?: {}", e.what()));
            throw;
        }
        return js;
    }
    json load_json_file(const std::string& filename) {
        throw_signal_ifneeded();
        json js;
        if(!is_file(filename)) {
            log_message("E1003", filename + " doesn't exist");
            throw FileNotFoundError(filename + " doesn't exist");
        }
        try {
            std::string data = file_get_contents(filename);
            clean_json_comments(data);
            js = json::parse(data);
        } catch(std::exception& e) {
            log_message("E1004", csd::str_format("{}: {}", filename, e.what()));
            throw;
        }
        return js;
    }
    int64_t file_size(const std::string &path) {
        struct ::stat sb;
        if(stat(path.c_str(), &sb) == 0) {
            return sb.st_size;
        }
        return 0;

    }

    bool is_file_same(const std::string& file_a, const std::string& file_b) {
        if(!is_file(file_a) || !is_file(file_b))
            return false;
        if(file_size(file_a) != file_size(file_b))
            return false;
        return file_get_contents(file_a) == file_get_contents(file_b);
    }

    bool file_put_contents(const std::string& filepath, const std::string& data) {
        throw_signal_ifneeded();
        if (filepath.empty()) {
            throw IOError("empty filepath given");
        }
        FILE* fp = fopen(filepath.c_str(), "wb");
        if(!fp) {
            throw IOError("could not open file for writing " + filepath);
            return false;
        }

        std::size_t transfered = fwrite(data.c_str(), 1, data.size(), fp);
        fclose(fp);
        if(transfered != data.size()) {
            throw IOError("failed to write to file " + filepath);
            return false;
        }
        return true;
    }

    bool file_put_contents_nl(const std::string& filepath, const std::string& data) {
        throw_signal_ifneeded();
        if (filepath.empty()) {
            throw IOError("empty filepath given");
        }
        FILE* fp = fopen(filepath.c_str(), "wb");
        if(!fp) {
            throw IOError("could not open file for writing " + filepath);
            return false;
        }

        std::size_t transfered = fwrite(data.c_str(), 1, data.size(), fp);
        if (data[data.size()-1] != '\n')
            fwrite("\n", 1, 1, fp);
        fclose(fp);
        if(transfered != data.size()) {
            throw IOError("failed to write to file " + filepath);
            return false;
        }
        return true;
    }

    std::string file_get_contents(const std::string& filepath) {
        int64_t size = file_size(filepath);
        FILE* fp = fopen(filepath.c_str(), "rb");
        if(!fp) {
            throw IOError("could not open file for reading: " + filepath);
        }
        std::vector<char> buffer(size+1);
        std::size_t transfered = fread(&buffer[0], 1, size, fp);
        fclose(fp);
        std::string result(buffer.data(), transfered);
        return result;
    }
    // scheme://user:pass@domain:port/path
    Url::Url(std::string url) {
        std::string::size_type scheme_end_pos = url.find(':');
        if(scheme_end_pos == std::string::npos) {
            // parse error
            return;
        }
        if(scheme_end_pos >= url.size()-3) {
            // parse error
            return;
        }
        scheme = url.substr(0, scheme_end_pos);
        url = url.substr(scheme_end_pos+3);

        std::string::size_type slash_pos = url.find('/');
        if(slash_pos == std::string::npos)
            slash_pos = url.size();
        else {
            path = url.substr(slash_pos);
            url = url.substr(0, slash_pos);
        }
        // url = user:pass@domain:port
        std::string::size_type at_pos = url.find('@');
        std::string::size_type pass_pos = url.find(':');
        std::string::size_type port_pos;
        if(at_pos == std::string::npos) {
            port_pos = pass_pos;
            pass_pos = 0;
        } else {
            port_pos = url.find(':', at_pos);
            if(pass_pos == port_pos) {
                pass_pos = at_pos;
            }
        }
        if(port_pos == std::string::npos)
            port_pos = url.size();
        user = url.substr(0, pass_pos);
        int domain_start = at_pos == std::string::npos? 0 : at_pos+1;
        domain = url.substr(domain_start, port_pos - domain_start);
        std::string port_str = port_pos < url.size()? url.substr(port_pos) : "";

        if(!port_str.empty())
            port = std::atoi(port_str.c_str());
        else
            port = 0; // 0 for default
    }

    bool is_zip(const std::string& file) {
        throw_signal_ifneeded();
        FILE* fp = fopen(file.c_str(), "rb");
        if(fp == nullptr)
            return false;
        unsigned char bytes[] = {0x50, 0x4B, 0x03, 0x04};
        unsigned char buffer[4] = {0};
        [[maybe_unused]] std::size_t unused = fread(buffer, 1, 4, fp);
        fclose(fp);
        return std::memcmp(buffer, bytes, 4) == 0;
    }

    std::vector<std::string> scan_dir(const std::string& output_dir) {
        std::vector<std::string> result;
        for(const std::filesystem::directory_entry& entry : DirIt(output_dir)) {
            result.push_back(entry.path().filename().string());
        }
        return result;
    }
    bool is_subdir(std::string parent, std::string child) {
        assert(parent[0] == '/');
        assert(child[0] == '/');

        if(parent[parent.size()-1] != '/')
            parent += '/';
        if(child[child.size()-1] != '/')
            child += '/';
        
        if(parent.size() > child.size())
            return false;
        
        if(parent == child)
            return true;
        return child.substr(0, parent.size()) == parent;
        
    }

    std::string join_path(std::string parent, std::string child) {
        if(child.empty())
            return parent;
        if(child == ".")
            return parent;
        if(child.find(':') != std::string::npos) {
            std::string message = "child path '" + child + "' cannot have ':'";
            tea::log_message("F1001", message);
        }
        parent = clean_path(parent);
        child = clean_path(child);
        while(child.size() >= 2) {
            if(child[0] == '.' && child[1] == '/') {
                child = child.substr(2);
            } else {
                break;
            }
        }
        if(parent[parent.size()-1] == '/') {
            if(child[0] == '/')
                parent += child.substr(1);
            else if(child[0] == '.' && child[1] == '/')
                parent += child.substr(2);
            else
                parent += child;
            return parent;
        }
        if(child[0] == '/')
            parent += child;
        else if(child[0] == '.' && child[1] == '/') {
            parent += child.substr(1);
        } else {
            parent += '/';
            parent += child;
        }

        return parent;
    }

    std::string getenv(const std::string& var) {
        throw_signal_ifneeded();
        const char* ptr = ::getenv(var.c_str());
        if(ptr == nullptr)
            return "";
        return ptr;
    }

    std::string home_dir() {
#ifdef _WIN32
        return  clean_path(tea::getenv("HOMEDRIVE") + tea::getenv("HOMEPATH"));
#else
        return tea::getenv("HOME");
#endif
    }

    fs::path first_sub_dir(fs::path path) {
        std::vector<std::string> bad_names {
            ".DS_Store",
            "__pycache__",
            ".AppleDB,",
            ".AppleDesktop"
        };
        for (const fs::directory_entry& entry : fs::directory_iterator(path)) {
            bool ignore = false;
            for(auto& bad_name : bad_names) {
                if(entry.path().filename() == bad_name) {
                    ignore = true;
                    break;
                }
            }
            if(ignore)
                continue;
            if(entry.is_directory())
                continue;
            return entry.path();
        }
        return path;
    }
    fs::path find_nest(fs::path path) {
        std::vector<std::string> bad_names {
            ".DS_Store",
            "__pycache__",
            ".AppleDB,",
            ".AppleDesktop",
            "__MACOSX"
        };
        while(true) {
            fs::path sub_dir;
            bool empty = true;
            for (const fs::directory_entry& entry : fs::directory_iterator(path)) {
                bool ignore = false;
                for(auto& bad_name : bad_names) {
                    if(entry.path().filename() == bad_name) {
                        ignore = true;
                        break;
                    }
                }
                if(ignore)
                    continue;
                if(!sub_dir.empty()) {
                    // too many files can't go further
                    return path;
                }
                if(!entry.is_directory())
                    return path;
                sub_dir = entry.path();
                empty = false;
            }
            path = sub_dir;
            if(empty)
                break;
        }
        return path;

    }

    std::string random_name(int len) {
        std::random_device dev;
        std::mt19937 rng(dev());
        std::uniform_int_distribution<std::mt19937::result_type> dist(0,26); // distribution in range [1, 6]

        std::string result;
        for(int i = 0; i < len; ++i) {
            char c = dist(rng) + 'a';
            result += c;
        }
        return result;
    }

    void remove_dir_nests(fs::path path) {
        fs::path src_dir = find_nest(path);
        if(src_dir == path) {
            return;
        }

        // first we have to renave current directory
        fs::path dir_to_remove = first_sub_dir(path);
        fs::path dst;
        std::string tmp_subdir;
        int min_len = 6;
        do {
            tmp_subdir = random_name(min_len);
            dst = path / tmp_subdir;
        } while(path_exists(src_dir / tmp_subdir));

        fs::rename(dir_to_remove, dst);
        dir_to_remove = dst;
        src_dir = find_nest(dst);
        std::vector<fs::path> to_move;
        // we are modifying dirtory so just to be safe we will store all files
        // in memory before we move them.
        for(const fs::directory_entry& entry : fs::directory_iterator(src_dir)) {
            to_move.push_back(entry.path());
        }

        for(fs::path& src_path : to_move) {
            fs::rename(src_path, dst / src_path.filename());
        }
        tea::rmdir(dir_to_remove);
    }

}