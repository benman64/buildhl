#pragma once

// We just need to forward declare FILE* which is not possible. So we must
// include this header
#include <cstdio>

#include <iosfwd>
#include <ios>

#include "iostream.hpp"

namespace ios {

/** @cond PRIVATE */
/* the libstd++ source files have __file_type which abstracts away OS specific
   API. This bellow is a poor reimplementation ontop of C FILE. This class
   is meant for easier adapting into libstd++. It appears __file_type is
   not buffured. This class will behave so.

   This will be OS specific so it's core implementation is not in the header.
   But it could be in the header if done right. Looking at existing libstd++
   they haven't put the core implementation in the header so we will do
   similar.
*/
class FileType {
public:
    typedef FILE *native_handle_type;
    FileType();
    ~FileType();

    FileType(FileType&& other) {
        (*this) = std::move(other);
    }
    FileType &operator=(FileType&& other) {
        mFile = other.mFile;
        other.mFile = nullptr;
        return *this;
    }
    FileType(const FileType&)=delete;
    FileType &operator= (const FileType&)=delete;
    explicit FileType(const std::string &fileName, std::ios_base::openmode openMode) {
        open(fileName, openMode);
    }
    bool open(const std::string &fileName, std::ios_base::openmode openMode);

    bool setBufferSize(std::streamsize size);

    /* equivalent of xsgetn */
    std::streamsize read(void *data, std::streamsize size);
    /* equivalent of xsputn */
    std::streamsize write(const void *data, std::streamsize size);

    std::streamsize write_2(const void* data1, std::streamsize size1,
        const void *data2, std::streamsize size2);
    std::streamoff seek(std::streamoff offset, std::ios_base::seekdir dir = std::ios_base::beg);

    std::streamoff tell() const;
    void close();
    bool isOpen() const;
private:
    native_handle_type mFile;
};

/** @endcond */

/** stream for reading files */
class file_istream : public istream_base {
public:
    file_istream()=default;
    file_istream(file_istream&&)=default;
    // truncate & append doesn't make sense here. ate could make sense
    // I think it's better to leave the parameter out and user can explicitly
    // just call seek to go to end
    /**
        @param fileName path to file to open for reading.
    */
    explicit file_istream(const std::string &fileName){
        open(fileName);
    }
    /** Opens file for reading.
        @param fileName path to file to open for reading.
    */
    bool open(const std::string &fileName) {
        return mFile.open(fileName, std::ios_base::in | std::ios_base::binary);
    }

    std::streamsize read(void *data, std::streamsize nbytes) {
        return mFile.read(data, nbytes);
    }
    std::streamoff seek(std::streamoff offset, std::ios_base::seekdir dir = std::ios_base::beg) {
        return mFile.seek(offset, dir);
    }
    std::streamoff tell() const { return mFile.tell(); }
    /** @return true if open. */
    bool is_open() const { return mFile.isOpen(); }
private:
    FileType mFile;
};

/** file stream for writing */
class file_ostream : public ostream_base {
public:
    file_ostream()=default;
    /** Opens file for writing.
        @param fileName path to file name. Will be created if not exists.
        @param openMode the openmode. Default is to truncate the file. binary
               mode is forced.
    */
    explicit file_ostream(const std::string &fileName, std::ios_base::openmode openMode=std::ios_base::trunc) {
        open(fileName, openMode | std::ios_base::binary);
    }
    /** Opens file for writing.
        @param fileName path to file name. Will be created if not exists.
        @param openMode the openmode. binary mode is forced.
    */
    bool open(const std::string &fileName, std::ios_base::openmode openMode) {
        return mFile.open(fileName, openMode | std::ios_base::out | std::ios_base::binary);
    }
    std::streamsize write(const void *data, std::streamsize nbytes) {
        return mFile.write(data, nbytes);
    }
    std::streamoff tell() const { return mFile.tell(); }
    /** @return true if file is open */
    bool is_open() const { return mFile.isOpen(); }
private:
    FileType mFile;
};

/** for reading, writing & seeking into a file */
class file_iostream : public iostream_base {
public:
    file_iostream(){}
    /**
        @param fileName path to file.
        @param openMode default is read/write, binary is forced. File will be
                        created if not exists.
    */
    explicit file_iostream(const std::string &fileName,
        std::ios_base::openmode openMode=std::ios_base::in |
            std::ios_base::out) {
        open(fileName, openMode);
    }
    /**
        @param fileName path to file.
        @param openMode binary is forced. File will be created if not exists.
    */
    bool open(const std::string &fileName, std::ios_base::openmode openMode) {
        return mFile.open(fileName, openMode | std::ios_base::binary);
    }
    std::streamsize read(void *data, std::streamsize nbytes) {
        return mFile.read(data, nbytes);
    }
    std::streamsize write(const void *data, std::streamsize nbytes) {
        return mFile.write(data, nbytes);
    }
    std::streamoff seek(std::streamoff offset, std::ios_base::seekdir dir = std::ios_base::beg) {
        return mFile.seek(offset, dir);
    }
    std::streamoff tell() const { return mFile.tell(); }
    /** @return true if file is open. */
    bool is_open() const { return mFile.isOpen(); }

private:
    FileType mFile;

};

}
