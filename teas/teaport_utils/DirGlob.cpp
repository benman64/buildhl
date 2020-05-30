#include "DirGlob.hpp"

#include "Log.hpp"

namespace tea {
    DirGlobExpression::DirGlobExpression(std::string expression) {
        if(expression[0] != '/')
            expression.insert(0, 1, '/');
        std::string::size_type pos = expression.find("/*/");
        mRecursive = pos != std::string::npos;
        if(mRecursive)
            mRootDir = expression.substr(0, pos);
        std::string::size_type slash_pos = expression.rfind('/');
        if(slash_pos != std::string::npos) {
            if(!mRecursive) {
                mRootDir = expression.substr(0, slash_pos);
            }
            expression = expression.substr(slash_pos+1);
        } else {
            mRootDir = "/";
        }

        if(mRootDir.empty() || mRootDir[mRootDir.size()-1] != '/')
            mRootDir += '/';
        mRegex = std::regex(expression);
    }

    bool starts_with(const std::string& str, const std::string& with) {
        if(with.size() > str.size())
            return false;
        return str.substr(0, with.size()) == with;
    }
    bool path_equal(std::string a, std::string b) {
        if(a[a.size()-1] != '/')
            a += '/';
        if(b[b.size()-1] != '/')
            b += '/';
        return a == b;
    }
    bool DirGlobExpression::match(std::string path) const {
        if(path[0] != '/') {
            path.insert(0, 1, '/');
        }
        if(!is_subdir(mRootDir, path))
            return false;

        if(!mRecursive) {
            if(!path_equal(dirname(path), mRootDir))
                return false;
        }
        path = basename(path);

        return std::regex_match(path, mRegex);
    }


    FileIterator::FileIterator(DirGlob& dir_glob, const std::string& root_dir) {
        mDirGlob = &dir_glob;
        mRootDir = root_dir;
        if(mRootDir[mRootDir.size()-1] == '/') {
            mRootDir.resize(mRootDir.size()-1);
        }
        std::string start_dir = join_path(mRootDir, dir_glob.root_dir());
        mDirIt = RecursiveDirIt(start_dir, std::filesystem::directory_options::follow_directory_symlink);
        // first time we recurse into the directory
        if(mDirIt != end(mDirIt) && mDirIt->is_directory())
            ++mDirIt;
        while(mDirIt != end(mDirIt) && mDirIt->is_directory()) {
            if(!mDirGlob->is_recursive()) {
                mDirIt.disable_recursion_pending();
            }
            ++mDirIt;
        }
        std::string path = mDirIt->path().string();
        path = path.substr(mRootDir.size());
        if(!mDirGlob->match(path)) {
            ++(*this);
        }
    }

    FileIterator& FileIterator::operator++() {
        while(mDirIt != end(mDirIt)) {
            if(!mDirGlob->is_recursive()) {
                mDirIt.disable_recursion_pending();
            }

            do {
                ++mDirIt;
            } while(mDirIt != end(mDirIt) && mDirIt->is_directory());
            if(mDirIt == end(mDirIt))
                break;
            std::string path = mDirIt->path().string();
            path = path.substr(mRootDir.size());
            if(mDirGlob->match(path))
                break;
        }
        return *this;
    }

    DirGlob::DirGlob(const std::string& exp) : mInclude(exp) {

    }
    FileIterator DirGlob::begin() {
        return FileIterator(*this, root_dir());
    }

    FileIterator DirGlob::end() const {
        return FileIterator();
    }

    bool DirGlob::copy_dir(const std::string& src, const std::string& dst, bool flat, bool symlink_files) {
        FileIterator it(*this, src);

        std::string root_dir = join_path(src, this->root_dir());
        int delete_count = root_dir.size();
        if(root_dir[root_dir.size()-1] != '/')
            ++delete_count;
        if(flat) {
            mkdir(dst);
        }
        std::map<std::string, bool> mkdir_cache;
        while(it) {
            std::string file = *it;
            std::string dst_file = flat? join_path(dst, basename(file)) : join_path(dst, file.substr(delete_count));
            std::string dst_dir = dirname(dst_file);
            if(mkdir_cache.count(dst_dir) == 0) {
                mkdir_p(dst_dir);
                mkdir_cache[dst_dir] = true;
            }
            if(symlink_files){
                if(!symlink(file, dst_file)) {
                    log_message("E1016", "could not create symlink at " + dst_file);
                    return false;
                }
            } else {
                if(!copy_file(file, dst_file)) {
                    log_message("E1017", "could copy " + file + " to " + dst_file);
                    return false;
                }
            }
            ++it;
        }
        return true;
    }
}