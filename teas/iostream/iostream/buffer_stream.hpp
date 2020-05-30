#pragma once

#include <iosfwd>
#include "iostream.hpp"
#include "memory_stream.hpp"

namespace ios {
// browsing libstd++ source code it seams they already abstracted out buffering
// and file IO. It's just not part of public API.

/**
    this one is for streams that support read/write. The buffer type is
    templated so you can have a very custom buffer. Like one that uses a
    specific memory location or something. Buffer stream must have the following

    @code
    class buffer {
    public:
        // truncate the buffer stream to the specific size.
        void truncate(std::streamsize size);

        // get the current capacity of the stream.
        std::streamsize capacity();
        // just like any ostream. Write size bytes. return how many bytes
        // written. Can be smaller if stream becomes full.
        std::streamsize write(const void *data, std::streamsize size);

        // like istream. Read size bytes into data. may be less read if stream
        // reaches eof.
        std::streamsize read(void *data, std::streamsize size);

        // return current position in the stream.
        std::streamoff tell();

        // return current size of stream. capacity() is total number of bytes
        // the stream has to work with. size() is number of bytes currently
        // written in the stream. E.g. sort of like a file size is size of the
        // file, and capacity is total memory allocated on disk to the file.
        std::streamsize size();

        // seeking within the stream. Returns new offset. Should be clamp
        // from 0 to size().
        std::streamoff seek(std::streamoff offset, std::ios_base::seekdir dir);

        // optional: sets the capacity of internal buffer. If not provided
        // then you cannot call buffer_iostream::set_capacity() or related
        // overloads.
        void set_capacity(std::streamsize size);

    };

    @endcode
*/
template<typename T, typename Buffer=memory_iostream<>>
class buffer_iostream : public iostream_base {
public:
    /** initializes with a sensible buffer capacity */
    buffer_iostream(){
        set_default_capacity();
    }
    ~buffer_iostream() {
        flush();
    }
    buffer_iostream              (buffer_iostream&&)      = default;
    buffer_iostream &operator=   (buffer_iostream&&)      = default;
    buffer_iostream              (const buffer_iostream&) = delete;
    buffer_iostream &operator=   (const buffer_iostream&) = delete;

    /** Initializes with a sensible buffer capacity */
    explicit buffer_iostream(T &&stream) : mStream(std::move(stream)) {
        set_default_capacity();
    }

    /** Once reading/writing begins calling this is not allowed */
    void set_buffer_size(std::streamsize size) {
        if(size == mBuffer.capacity()) {
            return;
        }
        flush();
        mBuffer.set_capacity(size);
    }
    /* get/set is needed for chaining */
    void    set_istream(T &&stream) { set_stream(std::move(stream)); }
    void    set_ostream(T &&stream) { set_stream(std::move(stream)); }
    T&      get_istream()           { return get_stream(); }
    T&      get_ostream()           { return get_stream(); }

    void set_stream(T &&stream) {
        flush();
        mStream = std::move(stream);
        mWasReading = mWasWriting = false;
        mBuffer.truncate(0);
        mOffset = mStream.tell();
    }

    T &get_stream() {
        return mStream;
    }

    std::streamsize write(const void *data, std::streamsize nBytes) {
        if(mWasReading) {
            mOffset += mBuffer.tell();
            mBuffer.truncate(0);
            mWasReading = false;
            mStream.seek(mOffset);
        }
        mWasWriting = true;
        std::streamsize writeBytes = 0;
        // no reason to do buffering. Just flush our contents and send
        // the data directly to the stream and exit.
        if(nBytes >= mBuffer.capacity()) {
            flush();
            writeBytes = mStream.write(data, nBytes);
            return writeBytes;
        }
        writeBytes = mBuffer.write(data, nBytes);
        if(writeBytes < nBytes) {
            flush();
            const void *data2 = reinterpret_cast<const uint8_t*>(data)+writeBytes;
            writeBytes += mBuffer.write(data2, nBytes - writeBytes);
            assert(writeBytes == nBytes);
        }
        return writeBytes;
    }

    std::streamsize read(void *data, std::streamsize nBytes) {
        if(mWasWriting) {
            flush();
            mWasWriting = false;
        }
        mWasReading = true;
        std::streamsize readBytes = 0;
        memory_helper_iostream output(data, nBytes);
        // first read what we currently have
        readBytes = mBuffer.read(output.cursor(), nBytes);
        if(readBytes == nBytes) {
            return readBytes;
        }
        output.seek(readBytes, std::ios_base::cur);
        // this means nBuffer has been fully read

        if((output.size() - output.tell()) >= mBuffer.capacity()) {
            std::streamsize read2 = mStream.read(output.cursor(), output.size()-output.tell());
            if(read2 > 0) {
                readBytes += read2;
            } else {
                /// @todo what to do about this error?
            }

            mBuffer.truncate(0);
            mOffset = mStream.tell();
            return readBytes;
        }

        // it should take atmost 2 buffered reads. Usually just 1.
        for(int i = 0; i < 2; ++i)  {
            // we are done yay :D
            if(output.size() == output.tell()) {
                break;
            }
            fillBufferWithReadData();
            std::streamsize read2 = mBuffer.read(output.cursor(), output.size()-output.tell());
            if(read2 <= 0) {
                break;
            }
            readBytes += read2;
            output.seek(read2, std::ios_base::cur);
        }
        return readBytes;
    }

    /** Flush the stream.

        @returns true on success. Sometimes there is nothing to actually be
                 done. In that case returns true because it was succesfull doing
                 nothing.
    */
    bool flush() {
        // if did not write any data, then nothing to flush
        if(!mWasWriting) {
            return true;
        }
        if(mBuffer.tell() == 0){
            return true;
        }
        std::streamsize writeBytes = mStream.write(mBuffer.data(), mBuffer.tell());
        /// @todo what to do about errors?
        bool success = writeBytes == mBuffer.tell();
        mOffset = mStream.tell();
        mBuffer.seek(0);
        return success;
    }

    std::streamoff seek(std::streamoff offset, std::ios_base::seekdir dir=std::ios_base::beg) {
        flush();
        std::streamoff computedOffset = offset;
        std::streamoff relative = 0;
        switch(dir) {
        default:
            // really it's error for default case
        case std::ios_base::beg: relative = 0; break;
        case std::ios_base::cur: relative = mOffset + mBuffer.tell(); break;
        case std::ios_base::end:
            // stream dependant
            mOffset = mStream.seek(offset, dir);
            mBuffer.truncate(0);
            mWasWriting = mWasReading = false;
            return mOffset;
        }
        computedOffset = relative + offset;
        if(mWasReading) {
            // check if it's within the buffer
            std::streamoff endOffset = mOffset + mBuffer.size();
            if(computedOffset >= mOffset && computedOffset <= endOffset) {
                mBuffer.seek(computedOffset - mOffset);
                return computedOffset;
            }
        }
        mOffset = mStream.seek(offset, dir);
        mWasReading = mWasWriting = false;
        mBuffer.truncate(0);
        return mOffset;
    }

    std::streamoff tell() const {
        return mOffset + mBuffer.tell();
    }
private:
    void fillBufferWithReadData() {
        if(mBuffer.tell() == mBuffer.size()) {
            mOffset += mBuffer.size();
            mBuffer.truncate(0);
        }
        std::streamsize bytesToFull = mBuffer.capacity() - mBuffer.size();
        if(bytesToFull == 0) {
            return;
        }
        std::streamoff originalOffset = mBuffer.tell();
        mBuffer.seek(0, std::ios_base::end);
        std::streamsize bRead = mStream.read(mBuffer.cursor(), bytesToFull);

        if(bRead > 0) {
            mBuffer.skip_write(bRead);
        } else {
            // error failed to fill buffer
            /// @todo what to do about this error?
        }
        mBuffer.seek(originalOffset);
    }

    // Do nothing stream already has a defined capacity
    void set_default_capacity() {
        if(mBuffer.capacity() <= 0) {
            set_buffer_size(1 << 10);
        }
    }

    T mStream;
    Buffer mBuffer;
    std::streamoff mOffset = 0;
    bool mWasWriting = false;
    bool mWasReading = false;

};


/** @cond PRIVATE */
// this is private helper class
template<typename T>
class BufferInputStreamHelper {
public:
    explicit BufferInputStreamHelper(T &&stream) : mStream(std::move(stream)) {

    }

    std::streamsize read(void *data, std::streamsize nBytes) {
        std::streamsize readBytes = mStream.read(data, nBytes);
        if(readBytes > 0) {
            mOffset += readBytes;
        }
        return readBytes;
    }
    std::streamoff tell() const {
        return mOffset;
    }

    std::streamsize write(const void*data, std::streamsize nbytes) {
        (void)data;
        (void)nbytes;
        assert(false);
        return 0;
    }

    T &get_istream() {
        return mStream;
    }
private:
    T mStream;
    std::streamoff mOffset = 0;
};

/** @endcond */

template<typename T>
class buffer_istream : public istream_base {
public:
    explicit buffer_istream(T &&stream)
        : mStream(BufferInputStreamHelper<T>(std::move(stream))) {

    }

    std::streamsize read(void *data, std::streamsize nBytes) {
        return mStream.read(data, nBytes);
    }

    void set_buffer_size(std::streamsize size) {
        mStream.set_buffer_size(size);
    }

    std::streamoff tell() const {
        return mStream.tell();
    }

    /* get/set is needed for chaining */

    void set_istream(T &&stream) {
        mStream.set_istream(BufferInputStreamHelper<T>(std::move(stream)));
    }

    T &get_istream() {
        return mStream.get_istream().get_istream();
    }
private:
    buffer_iostream<BufferInputStreamHelper<T>> mStream;
};

/** @cond PRIVATE */
// This is private helper class.
template<typename T>
class BufferOutputStreamHelper {
public:
    explicit BufferOutputStreamHelper(T &&stream) : mStream(std::move(stream)) {

    }

    std::streamsize read(void *data, std::streamsize nBytes) {
        assert(false);
        return 0;
    }
    std::streamoff tell() {
        return mOffset;
    }

    std::streamoff seek(std::streamoff offset, std::ios_base::seekdir dir=std::ios_base::beg) {
        (void)dir;
        (void)offset;
        assert(mOffset == 0);
        assert(offset = 0);
        assert(dir == std::ios_base::beg);
        return mOffset;
    }

    std::streamsize write(const void*data, std::streamsize nbytes) {
        std::streamsize writeBytes = mStream.write(data, nbytes);
        if(writeBytes > 0) {
            mOffset += writeBytes;
        }
        return writeBytes;
    }

    T &get_ostream() {
        return mStream;
    }
private:
    T mStream;
    std::streamoff mOffset = 0;
};
/** @endcond */

template<typename T>
class buffer_ostream : public ostream_base {
public:
    explicit buffer_ostream(T &&stream)
        : mStream(BufferOutputStreamHelper<T>(std::move(stream))) {

    }

    std::streamsize write(const void *data, std::streamsize nBytes) {
        return mStream.write(data, nBytes);
    }

    void set_buffer_size(std::streamsize size) {
        mStream.set_buffer_size(size);
    }

    void flush() {
        mStream.flush();
    }

    /* get/set is needed for chaining */

    void set_ostream(T &&stream) {
        mStream.set_ostream(BufferOutputStreamHelper<T>(std::move(stream)));
    }

    T &get_ostream() {
        return mStream.get_ostream().get_ostream();
    }
private:
    buffer_iostream<BufferOutputStreamHelper<T>> mStream;
};

}
