#pragma once

#include <limits>
#include "iostream.hpp"

namespace ios {

/** Similar to "array_view" like concept. To limit reading/writing to a specific
    region in the underlining stream. Does not take ownership of stream.
*/
template<typename T>
class sub_iostream : public iostream_base {
public:
    sub_iostream(sub_iostream&& other) {
        *this = std::move(other);
    }
    sub_iostream& operator= (sub_iostream&& other) {
        mStream         = other.mStream;
        mMin            = other.mMin;
        mMax            = other.mMax;

        other.mStream   = nullptr;
        other.mMin      = 0;
        other.mMax      = 0;
        return *this;
    }
    sub_iostream            (const sub_iostream&)   =delete;
    sub_iostream& operator= (const sub_iostream&)   =delete;

    sub_iostream() {
        mStream = nullptr;
        mMin    = 0;
        mMax    = 0;
    }
    /** Limits stream from current all the way to end of stream.

        @param stream the stream to limit. Ownership is not taken and stream
           must be valid for the lifetime of this object.
    */
    explicit sub_iostream(T &stream) : mStream(&stream) {
        mMin = stream.tell();
        mMax = std::numeric_limits<std::streamoff>::max();
    }

    /**
        @param stream the stream to limit. Ownership is not taken and stream
               must be valid for the lifetime of this object.
        @param min starting bytes of allowable reading/writing.
        @param max 1 past the end in bytes of allowable reading/writing.
    */
    explicit sub_iostream(T &stream, std::streamoff min, std::streamoff max) : mStream(&stream) {
        mMin = min;
        mMax = max;
    }


    std::streamsize read(void *data, std::streamsize size) {
        if(mStream == nullptr) {
            return 0;
        }
        size = std::min(size, mMax - mStream->tell());
        if(size <= 0) {
            return 0;
        }
        std::streamsize didRead = mStream->read(data, size);
        return didRead;
    }

    std::streamsize write(const void *data, std::streamsize size) {
        if(mStream == nullptr) {
            return 0;
        }
        size = std::min(size, mMax - mStream->tell());
        if(size <= 0) {
            return 0;
        }
        std::streamsize didWrite = mStream->write(data, size);
        return didWrite;
    }

    /** @return position within the stream relative to subsection */
    std::streamoff tell() const {
        if(mStream == nullptr) {
            // should we return 0 or make this case be undefined?
            return 0;
        }
        return mStream->tell() - mMin;
    }

    /** @return size relative to subsection */
    std::streamsize size() const {
        if(mStream == nullptr) {
            return 0;
        }
        std::streamsize size = mStream->size();
        size = std::min(size, mMax);
        return size - mMin;
    }
    /** Seeks only within the subsection.

        @param offset offset amount
        @param dir for std:ios_base::beg 0 is at start of subjection, end is at
                   end of subsection.
    */
    std::streamoff seek(std::streamoff offset, std::ios_base::seekdir dir = std::ios_base::beg) {
        if(mStream == nullptr) {
            return 0;
        }
        switch(dir) {
        default:
        case std::ios_base::beg: break;
        case std::ios_base::cur: offset += tell(); break;
        case std::ios_base::end: offset += size(); break;
        }

        offset += mMin;
        offset = clamp(offset, mMin, mMax);
        mStream->seek(offset, std::ios_base::beg);
        return tell();
    }
    bool has_stream() const {return mStream != nullptr;}
private:
    T*              mStream;
    std::streamoff  mMin;
    std::streamoff  mMax;
};

/** Limit reading of stream to a specific subsection. Does not take ownership
    of stream.
*/
template<typename T>
class sub_istream : public istream_base {
public:
    sub_istream(sub_istream&& other) {
        *this = std::move(other);
    }
    sub_istream& operator=(sub_istream&& other) {
        mStream         = other.mStream;
        mSize           = other.mSize;
        mOffset         = other.mOffset;

        other.mStream   = nullptr;
        other.mSize     = 0;
        other.mOffset   = 0;
        return *this;
    }
    sub_istream             (const sub_istream&)    =delete;
    sub_istream& operator=  (const sub_istream&)    =delete;

    /**
        @param stream the stream to limit. Ownership is not taken and stream
               must be valid for the lifetime of this object.
        @param size the size in bytes to limit reading.
    */
    explicit sub_istream(T &stream, std::streamsize size=std::numeric_limits<std::streamsize>::max())
        : mStream(&stream) {
        mSize = size;
    }

    /** @return position of stream relative to subsection */
    std::streamoff tell() const {
        return mOffset;
    }

    std::streamsize read(void *data, std::streamsize size) {
        size = std::min(size, mSize - tell());
        if(size <= 0) {
            return 0;
        }
        std::streamsize didRead = mStream->read(data, size);
        if(didRead > 0) {
            mOffset += didRead;
        }
        assert(mOffset <= mSize);
        return didRead;
    }
private:
    T*              mStream;
    std::streamsize mSize;
    std::streamsize mOffset = 0;
};

/** Limits writing of stream to a specific size. Does not take ownership of
    stream.
*/
template<typename T>
class sub_ostream : public ostream_base {
public:
    sub_ostream(sub_ostream&& other) {
        *this = std::move(other);
    }
    sub_ostream& operator=(sub_ostream&& other) {
        mStream         = other.mStream;
        mSize           = other.mSize;
        mOffset         = other.mOffset;

        other.mStream   = nullptr;
        other.mSize     = 0;
        other.mOffset   = 0;
        return *this;
    }
    sub_ostream             (const sub_ostream&)    =delete;
    sub_ostream& operator=  (const sub_ostream&)    =delete;

    /**
        @param stream the stream to limit. Ownership is not taken and stream
               must be valid for the lifetime of this object.
        @param size the amount of bytes to limit writing.
    */
    explicit sub_ostream(T &stream, std::streamsize size=std::numeric_limits<std::streamsize>::max())
        : mStream(&stream) {
        mSize = size;
    }

    /** @return The current offset relative to subsection */
    std::streamoff tell() const {
        return mOffset;
    }

    std::streamsize write(const void *data, std::streamsize size) {
        size = std::min(size, mSize - tell());
        if(size <= 0) {
            return 0;
        }
        std::streamsize didWrite = mStream->write(data, size);
        if(didWrite > 0) {
            mOffset += didWrite;
        }
        assert(mOffset <= mSize);
        return didWrite;
    }
private:
    T*              mStream;
    std::streamsize mSize;
    std::streamsize mOffset = 0;
};

}
