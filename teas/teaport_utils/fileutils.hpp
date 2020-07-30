#pragma once

#include <string>
#include <vector>
#include <list>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <cstdio>

#include "exceptions.hpp"
#include "ExitTransaction.hpp"

namespace tea {
    class CFile {
    public:
        explicit CFile(FILE* fp);
        ~CFile();
        CFile(const CFile&)=delete;
        CFile& operator=(const CFile&)=delete;

        std::string read_str(int size);
        int read(void* buffer, int size);
        int write(const void* buffer, int size);
        void close();
        FILE* get_handle() const {return mFile;}

        explicit operator bool() const {return mFile != nullptr;}
    private:
        FILE* mFile;
    };
    bool is_file(const std::string &path);
    bool is_link(const std::string& path);
    bool is_dir(const std::string& path);
    std::string absdir(std::string dir, std::string relativeTo={});
    bool path_exists(const std::string& path);


    bool mkdir(const std::string& dir, int mode = 0700);
    bool mkdir_p(const std::string& dir);
    std::string getcwd();
    bool chdir(const std::string& path);
    inline bool chdir(const char* path) {
        std::string str = path;
        return tea::chdir(str);
    }
    std::string dirname(std::string path);
    std::string basename(std::string path);

    bool files_readonly_recurse(const std::filesystem::path& path);

    bool rmdir(const std::string& dir);
    bool rmdir(const std::filesystem::path& path);
    inline bool rmdir(const char* str) {return rmdir(std::filesystem::path(str));}
    std::string expand_path(std::string path);
    bool is_absolute_path(const std::string& path);
    std::string clean_path(std::string path);

    bool copy_file(const std::string& src, const std::string& dst);
    bool copy_dir(std::string src, std::string dst);
    bool symlink(std::string src, std::string dst);
    bool is_zip(const std::string& file);
    std::vector<std::string> scan_dir(const std::string& path);
    bool is_subdir(std::string parent, std::string child);
    std::string join_path(std::string parent, std::string child);
    std::string getenv(const std::string& var);
    std::string home_dir();

    // ignores comments both multiline & single line.
    nlohmann::json parse_json(std::string js);
    nlohmann::json load_json_file(const std::string& filename);

    int64_t file_size(const std::string& path);
    bool is_file_same(const std::string& file_a, const std::string& file_b);
    bool file_put_contents(const std::string& path, const std::string& data);
    bool file_put_contents_nl(const std::string& filepath, const std::string& data);

    std::string file_get_contents(const std::string& path);
    std::filesystem::path find_nest(std::filesystem::path path);
    void remove_dir_nests(std::filesystem::path path);
    class TmpPath {
    public:
        explicit TmpPath(std::string path) {
            mPath = absdir(path);
            mExit = ExitTransaction([path]{
                tea::rmdir(path);
            });
        }
        TmpPath(const TmpPath&)=delete;
        TmpPath& operator=(const TmpPath&)=delete;
        ~TmpPath(){
            if(mRm)
                tea::rmdir(mPath);
            mExit.success();
        }
        void disown() {mRm = false; mExit.success();}
    private:
        std::string mPath;
        bool mRm    = true;
        ExitTransaction mExit;
    };

    using DirIt = std::filesystem::directory_iterator;
    using RecursiveDirIt = std::filesystem::recursive_directory_iterator;
    /** scheme://user:pass@domain:port/path */
    class Url {
    public:
        Url(){}
        Url(std::string url);
        std::string scheme;
        std::string path;
        std::string user;
        std::string domain;
        int port = 0;
    };
}