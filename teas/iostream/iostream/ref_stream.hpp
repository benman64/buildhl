#pragma once

#include "iostream.hpp"

namespace ios {

/** Holds a reference to the stream. This allows for a stream to not own
    a base stream. For example when linking up in a filter.
*/
template<typename T>
class ref_istream : public istream_base {
public:
    ref_istream(ref_istream &&other) : mStream(other.mStream){
        other.mStream = nullptr;
    }
    ref_istream(const ref_istream& other)=delete;
    explicit ref_istream(T &stream): mStream(&stream) {
    }

    std::streamsize read(void* data, std::streamsize nBytes) {
        return mStream->read(data, nBytes);
    }

private:
    T* mStream;
};

/** Holds a reference to the stream. This allows for a stream to not own
    a base stream. For example when linking up in a filter.
*/
template<typename T>
class ref_ostream : public ostream_base {
public:
    ref_ostream(ref_ostream &&other) : mStream(other.mStream){
        other.mStream = nullptr;
    }
    ref_ostream(const ref_ostream& other)=delete;

    explicit ref_ostream(T &stream): mStream(&stream) {
    }

    std::streamsize write(const void* data, std::streamsize nBytes) {
        return mStream->write(data, nBytes);
    }


private:
    T* mStream;
};








/** Holds a pointer to the stream. This allows for a stream to not own
    a base stream. For example when linking up in a filter.
*/
template<typename T>
class ptr_istream : public istream_base {
public:
    explicit ptr_istream(T &stream): mStream(&stream) {
    }

    std::streamsize read(void* data, std::streamsize nBytes) {
        return mStream->read(data, nBytes);
    }


    void link(T &stream) {
        mStream = &stream;
    }
private:
    T *mStream;
};

/** Holds a pointer to the stream. This allows for a stream to not own
    a base stream. For example when linking up in a filter.
*/
template<typename T>
class ptr_ostream : public ostream_base {
public:
    ptr_ostream(T &stream): mStream(&stream) {
    }

    std::streamsize write(const void* data, std::streamsize nBytes) {
        return mStream->write(data, nBytes);
    }


    void link(T &stream) {
        mStream = &stream;
    }
private:
    T *mStream;
};


}
