#pragma once

#include "memory_stream.hpp"

namespace ios {
/** Block here means only reads N bytes at a time. Some streams & devices
    you can only read for example 512 bytes at a time. And anything more/less
    results in error.
*/
template<typename T, typename Buffer=memory_iostream<>>
class block_istream {
public:
    /**
        @param stream the stream to read from.
        @param blockSize will read exactly this size in bytes at a time.
    */
    explicit block_istream(T &&stream, std::streamsize blockSize)
        : mStream(std::move(stream)) {
        mBlockSize = blockSize;
        mBuffer.set_capacity(mBlockSize);
        mBuffer.skip_write(mBlockSize);
    }

    std::streamsize read(void *data, std::streamsize read) {
        if(mBuffer.tell() == mBuffer.capacity()){
            read_next_block();
        }

        return mBuffer.read(data, read);
    }

    /** reads the next block chunk. Usefull for when you want to skip blocks
        without reading them into a buffer.

        @return true if exactly block size of bytes have been read.
    */
    bool read_next_block() {
        mBuffer.truncate(0);
        mBuffer.set_capacity(mBlockSize);

        std::streamsize bRead = mStream.read(mBuffer.cursor(), mBuffer.capacity());
        mBuffer.skip_write(bRead);
        mBuffer.seek(0);
        if(bRead != mBlockSize) {
            // What to do throw an error?
            return false;
        }
        return true;
    }

    /** @return the size of the current block. */
    std::streamsize cur_block_size() {
        return mBuffer.size();
    }

    /* get/set is needed for chaining */

    void set_istream(T &&stream) {
        mStream = std::move(stream);
    }

    T &get_istream() {
        return mStream;
    }
private:
    Buffer mBuffer;
    std::streamsize mBlockSize;
    T mStream;
};


/** Block here means only reads N bytes at a time. Some streams & devices
    you can only read for example 512 bytes at a time. And anything more/less
    results in error.
*/
template<typename T, typename Buffer=memory_iostream<>>
class block_ostream {
public:
    /**
        @param stream the stream to write to
        @param blockSize will write exactly this amount to stream.
    */
    explicit block_ostream(T &&stream, std::streamsize blockSize)
        : mStream(std::move(stream)) {
        mBlockSize = blockSize;
        mBuffer.set_capacity(mBlockSize);
        mBuffer.truncate(0);
    }

    std::streamsize write(const void *dataIn, std::streamsize size) {
        const uint8_t *data = reinterpret_cast<const uint8_t*>(dataIn);

        std::streamsize totalWritten = 0;
        while(totalWritten < size) {
            std::streamsize remainder = size - totalWritten;
            std::streamsize bWrite = 0;
            if(mBuffer.tell() > 0 || remainder < mBlockSize) {
                bWrite = mBuffer.write(data, remainder);

                if(mBuffer.tell() == mBuffer.capacity()) {
                    write_next_block();
                }
                totalWritten += bWrite;
                data += bWrite;
                continue;
            }

            // we can write to stream directly as there is more than block
            // size available to write
            bWrite = mStream.write(data, mBlockSize);
            if(bWrite <= 0) {
                // error what to do?
                break;
            }
            data += bWrite;
            totalWritten += bWrite;
        }

        return totalWritten;
    }


    /* get/set is needed for chaining */

    void set_ostream(T &&stream) {
        mStream = std::move(stream);
    }

    T &get_ostream() {
        return mStream;
    }
private:
    bool write_next_block() {
        if(mBuffer.size() > 0) {
            std::streamsize bRead = mStream.write(mBuffer.data(), mBuffer.size());
            if(bRead != mBlockSize) {
                // What to do throw an error?
                return false;
            }
        }
        mBuffer.truncate(0);
        mBuffer.set_capacity(mBlockSize);
        return true;
    }

    Buffer mBuffer;
    std::streamsize mBlockSize;
    T mStream;
};

}
