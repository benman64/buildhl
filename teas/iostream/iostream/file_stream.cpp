#include <string>

#include "file_stream.hpp"
namespace ios {
    FileType::FileType() {
        mFile = nullptr;
    }
    FileType::~FileType() {
        close();
    }
    bool FileType::open(const std::string &fileName, std::ios_base::openmode openMode) {
        const bool truncate   = openMode & std::ios_base::trunc;
        const bool append     = openMode & std::ios_base::app;
        const bool read       = openMode & std::ios_base::in;
        const bool write      = openMode & std::ios_base::out;
        std::string mode;
        // truncate first
        if(truncate && append) {
            FILE *fp = fopen(fileName.c_str(), "wb");
            if(fp != nullptr){
                fclose(fp);
            }
        }
        if(append) {
            mode += 'a';
            if(read) {
                mode += '+';
            }
        } else if(truncate) {
            mode += 'w';
            if(read) {
                mode += '+';
            }
        } else {
            if(read && write) {
                mode += "r+";
            } else if(read) {
                mode += "r";
            } else if(write) {
                mode += "w";
            }
        }
        mode += 'b';
        mFile = fopen(fileName.c_str(), mode.c_str());
        if(mFile == nullptr && !truncate) {
            // try again, the file probably doesn't exist
            mode.clear();
            if(append) {
                mode += 'a';
            } else {
                mode += 'w';
            }
            if(read) {
                mode += '+';
            }
            mode += 'b';
            mFile = fopen(fileName.c_str(), mode.c_str());
        }
        if(mFile == nullptr) {
            /// @todo I don't know throw exception?
        } else {
            if(openMode & std::ios_base::ate) {
                fseek(mFile, 0, SEEK_END);
            }
            setBufferSize(0);
        }
        return mFile != nullptr;
    }

    bool FileType::setBufferSize(std::streamsize size) {
        return setvbuf(mFile, nullptr, _IOFBF, size) == 0;
    }

    /* equivalent of xsgetn */
    std::streamsize FileType::read(void *data, std::streamsize size) {
        return fread(data, 1, size, mFile);
    }
    /* equivalent of xsputn */
    std::streamsize FileType::write(const void *data, std::streamsize size) {
        return fwrite(data, 1, size, mFile);
    }

    /*
        equivalent of xsgetn_2. I assume there is an optimization trick that
        libstd++ is doing. I did not look in depth to see what that trick is.
        I went as far as here https://gcc.gnu.org/onlinedocs/libstdc++/libstdc++-html-USERS-4.0/basic__file_8h-source.html

        We'll have this here I guess. And possibly use it. Implemented as per
        my understanding. I assume it uses writev

        https://linux.die.net/man/2/writev

        Interesting libstd++ doesn't define xsgetn_2.
    */
    std::streamsize FileType::write_2(const void* data1, std::streamsize size1,
        const void *data2, std::streamsize size2) {
        std::streamsize totalWrite = 0;
        totalWrite = write(data1, size1);
        if(totalWrite == size1) {
            std::streamsize write2Bytes = write(data2, size2);
            if(write2Bytes > 0) {
                totalWrite += write2Bytes;
            }
        }
        return totalWrite;
    }
    std::streamoff FileType::seek(std::streamoff offset, std::ios_base::seekdir dir) {
        // @note fseek doesn't support 64bit on 32bit systems
        int seekType;
        switch(dir) {
        default:
            //error exception?
        case std::ios_base::beg: seekType = SEEK_SET; break;
        case std::ios_base::cur: seekType = SEEK_CUR; break;
        case std::ios_base::end: seekType = SEEK_END; break;
        }

        fseek(mFile, offset, seekType);
        return ftell(mFile);
    }

    std::streamoff FileType::tell() const {
        return ftell(mFile);
    }
    void FileType::close() {
        if(mFile != nullptr) {
            fclose(mFile);
            mFile = nullptr;
        }
    }

    bool FileType::isOpen() const {
        return mFile != nullptr;
    }
}
