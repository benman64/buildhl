#pragma once

#include <stdio.h>
#include <string>
#include <regex>

#include "fileutils.hpp"

namespace tea {


    class DirGlobExpression {
    public:
        DirGlobExpression(){};
        DirGlobExpression(std::string expression);
        bool match(std::string path) const;
        bool is_recursive() const {return mRecursive;}
        std::string root_dir() const {return mRootDir;}

        DirGlobExpression& operator=(const DirGlobExpression&)=default;
        DirGlobExpression& operator=(const std::string& str) {
            *this = DirGlobExpression(str);
            return *this;
        }
    private:
        std::string mRootDir;
        std::regex  mRegex;
        bool        mRecursive = false;
    };

    class DirGlob;
    class FileIterator {
    public:
        FileIterator(){};
        FileIterator(DirGlob& dir_glob, const std::string& root_dir);

        FileIterator& operator++();
        std::string operator*() {
            return mDirIt->path().string();
        }
        explicit operator bool() const {
            return mDirGlob != nullptr && (mDirIt != end(mDirIt));
        }
        RecursiveDirIt          mDirIt;
        DirGlob*                mDirGlob = nullptr;
        std::string             mRootDir;
    };


    class DirGlob {
    public:
        DirGlob(){}
        DirGlob(const std::string& dir_expression);
        void set_glob_expression(const std::string& exp) {
            mInclude = exp;
        }

        void exclude(const std::string& ignore) {
            mExclude.push_back(ignore);
        }
        bool match(const std::string& path) const {
            if(!mInclude.match(path))
                return false;
            for(const DirGlobExpression& expression : mExclude) {
                if(expression.match(path))
                    return false;
            }
            return true;
        }

        bool is_recursive() const {return mInclude.is_recursive();}
        std::string root_dir() const {return mInclude.root_dir();}

        FileIterator begin();
        FileIterator end() const;
        bool copy_dir(const std::string& src, const std::string& dst, bool flat=false, bool symlink=false);
    private:
        DirGlobExpression               mInclude;
        std::vector<DirGlobExpression>  mExclude;
    };
}