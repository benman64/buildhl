#pragma once

#include <memory>
#include <vector>

#include "iostream.hpp"

namespace ios {
/*
    3 streams we need here

    - memory_helper_iostream
    - memory_iostream
    - heap_iostream
*/

/** tag to not do any initialization when use with
    basic_memory_hellper_iostream
*/
class no_init {};

/** @cond PRIVATE */
namespace pimpl {
    template<int mask>
    bool no_mask(const void *what) {
        return (((uintptr_t)(what)) & mask) == 0;
    }

}

/** @endcond */

/** A helper for implementing streams that need to abstract reading & writing
    to a location in memory.

    This class treats a single block of memory as an iostream.
*/
template<typename pointer=uint8_t*>
class basic_memory_helper_iostream : public iostream_base {
public:
    basic_memory_helper_iostream() : basic_memory_helper_iostream<pointer>((pointer)nullptr, (pointer)nullptr) {}
    basic_memory_helper_iostream(const no_init&) {}
    /**
        @param start the first byte
        @param end 1 past the last byte in the sequence.
    */
    template<typename input_pointer=void*, typename input_pointer2=input_pointer>
    basic_memory_helper_iostream(input_pointer start, input_pointer end) {
        mStart          = reinterpret_cast<pointer>(start);
        mEnd            = reinterpret_cast<pointer>(end);
        mEndOfStorage   = reinterpret_cast<pointer>(end);
        mCursor         = mStart;
    }

    /**
        @param start pointer to first byte in the stream.
        @param size size in bytes of data block.
    */
    template<typename input_pointer=void*>
    basic_memory_helper_iostream(input_pointer start, std::streamsize size) {
        mStart          = reinterpret_cast<pointer>(start);
        mEnd            = mStart + size;
        mEndOfStorage   = mStart + size;
        mCursor         = mStart;
    }

    /** The size of this stream */
    std::streamsize size() const {
        return mEnd - mStart;
    }

    /** @return true if the cursor as at the end of the stream . */
    bool eof() const {
        return mCursor == mEnd;
    }

    /** The capacity of the stream */
    std::streamsize capacity() const {
        return mEndOfStorage - mStart;
    }


    std::streamsize write(const void *data, std::streamsize bSize) {
        using namespace pimpl;
        bSize = std::min(bSize, mEndOfStorage - mCursor);
        if(data != mCursor) {
            const uint8_t *data8 = reinterpret_cast<const uint8_t*>(data);

            // I really don't understand why doing this is faster. Ofcourse
            // tested with optimizations enabled
            if(bSize == 8 && no_mask<7>(data8)) {
                std::copy(data8, data8+8, mCursor);
            } else if(bSize == 4 && no_mask<3>(data8)) {
                std::copy(data8, data8+4, mCursor);
            } else if(bSize == 2 && no_mask<1>(data8)) {
                std::copy(data8, data8+2, mCursor);
            } else {
                std::copy(data8, data8+bSize, mCursor);
            }

        }
        mCursor += bSize;
        mEnd = std::max(mEnd, mCursor);
        return bSize;
    }

    /** Pretend to write to the stream increasing the size as needed & updating
        the cursor.

        @param bSize size in bytes to pretend to write.
        @return size written, could be less than @p bSize if capacity is exceeded
    */
    std::streamsize skip_write(std::streamsize bSize) {
        bSize = std::min(bSize, mEndOfStorage - mCursor);
        mCursor += bSize;
        mEnd = std::max(mEnd, mCursor);
        return bSize;
    }

    std::streamsize read(void *data, std::streamsize bSize) {
        uint8_t *endRead = mCursor + bSize;
        if(endRead > mEnd) {
            endRead = mEnd;
            bSize = mEnd - mCursor;
        }
        if(bSize == 0) {
            return 0;
        }
        std::copy(mCursor, mCursor+bSize, reinterpret_cast<uint8_t*>(data));
        mCursor += bSize;
        return bSize;
    }

    /** @return current position in the stream */
    std::streamsize tell() const {
        return mCursor - mStart;
    }

    std::streamoff seek(std::streamoff offset, std::ios_base::seekdir dir = std::ios_base::beg) {
        // @note fseek doesn't support 64bit on 32bit systems
        pointer relative;
        switch(dir) {
        default:
            /// @todo error exception?
        case std::ios_base::beg: relative = mStart;     break;
        case std::ios_base::cur: relative = mCursor;    break;
        case std::ios_base::end: relative = mEnd;       break;
        }

        mCursor = relative + offset;
        if(mCursor > mEnd) {
            mCursor = mEnd;
        } else if (mCursor < mStart) {
            mCursor = mStart;
        }
        return tell();
    }

    /** Sets where the eof marker is of the stream.

        @param size must be from 0 to capacity of stream.
    */
    void truncate(std::streamsize size) {
        mEnd = mStart + size;
        if(mEnd > mEndOfStorage) {
            mEnd = mEndOfStorage;
        }
        if(mCursor > mEnd) {
            mCursor = mEnd;
        }
    }

    /** @return pointer to where the read/write cursor is*/
    pointer cursor() const { return mCursor; }
    /** @return pointer to start of the data */
    pointer data() const { return mStart; }
private:
    pointer mStart; // the first byte
    pointer mEnd; // 1 past the last byte
    pointer mEndOfStorage; // 1 past the last byte.
    pointer mCursor; // next read will read this byte
};

/** Minimalistic class for reading data forward

    This should be a private class.

*/
class memory_section_istream {
public:
    typedef uint8_t value_type;
    memory_section_istream(const memory_section_istream&)=default;

    /**
        @param start the start of the data buffer.
        @param end 1 past the last byte of data buffer.
    */
    memory_section_istream(const void*start, const void*end) {
        mCursor = reinterpret_cast<const value_type*>(start);
        mEndOfStorage = reinterpret_cast<const value_type*>(end);
    }

    /**
        @param start the start of the data buffer.
        @param size the size in bytes of data buffer.
    */
    memory_section_istream(const void* start, std::streamsize size) {
        mCursor = reinterpret_cast<const value_type*>(start);
        mEndOfStorage = mCursor + size;
    }

    std::streamsize read(void *data, std::streamsize size) {
        const value_type *end = mCursor + size;
        if(end > mEndOfStorage) {
            end = mEndOfStorage;
            size = end - mCursor;
        }

        std::copy(mCursor, end, reinterpret_cast<value_type*>(data));
        mCursor = end;
        return size;
    }

    /** skip @p size bytes. Will not move cursor past end of storage.

        @param size size in bytes to skip.
    */
    void void_skip(std::streamsize size) {
        mCursor += size;
        if(mCursor > mEndOfStorage) {
            mCursor = mEndOfStorage;
        }
    }

    /** @return where cursor is pointing at */
    const value_type* cursor() {
        return mCursor;
    }

    /** set where the cursor to be */
    void set_cursor(const void *cursor) {
        mCursor = reinterpret_cast<const value_type*>(cursor);
    }

    /** set the end byte of the stream. */
    void set_end(const void *end) {
        mEndOfStorage = reinterpret_cast<const value_type*>(end);
    }

    /** @return true if no more data to read/write */
    bool eof() const {
        return mCursor == mEndOfStorage;
    }

    /**
        @return remaining size of the stream.
    */
    std::streamsize size() const {
        return mEndOfStorage - mCursor;
    }

    /** @return always returns 0 */
    std::streamoff tell() const {
        return 0;
    }
private:
    const value_type* mCursor;
    const value_type* mEndOfStorage;
};
typedef basic_memory_helper_iostream<uint8_t*> memory_helper_iostream;
typedef basic_memory_helper_iostream<const uint8_t*> const_memory_helper_iostream;


/** reads & writes to a non-growable block on the heap */
template<typename Allocator=std::allocator<uint8_t>>
class memory_iostream : public memory_helper_iostream {
public:

    /**
        @param size size in bytes to allocate for buffer.
        @param allocator the allocator to use.
    */
    template<typename Allocator2=Allocator>
    explicit memory_iostream(std::streamsize size, const Allocator2 &allocator = Allocator())
        : memory_helper_iostream(no_init()), mAllocator(allocator) {
        init_with_capacity(size);
    }
    /** initializes an empty stream */
    memory_iostream(){}
    explicit memory_iostream(std::streamsize size) :
        memory_helper_iostream(no_init()) {
        init_with_capacity(size);
    }

    ~memory_iostream() {
        mAllocator.deallocate(data(), capacity());
    }

    memory_iostream &operator=(memory_iostream &&other) {
        if(other.mAllocator != mAllocator) {
            mAllocator = std::move(other.mAllocator);
        }
        *static_cast<memory_helper_iostream*>(this) = other;
        *static_cast<memory_helper_iostream*>(&other) = memory_helper_iostream();

        return *this;
    }
    memory_iostream(const memory_iostream&)             =default;
    memory_iostream(memory_iostream&& other) { *this = std::move(other); }


    /** Resizes the stream. Old data is copied over

        @param newCapacity the new capacity.
        @param copy if true copies old data over. If false no copy is done
               and stream starts empty.
    */
    bool set_capacity(std::streamsize newCapacity, bool copy = true) {
        if(capacity() == newCapacity) {
            return true;
        }

        uint8_t *newData = mAllocator.allocate(newCapacity);
        if(data() != nullptr) {
            if(copy) {
                std::streamsize copySize = std::min(size(), newCapacity);
                std::copy(data(), data()+copySize, newData);
            }
            mAllocator.deallocate(data(), capacity());
        }

        std::streamsize offset = copy? tell() : 0;
        std::streamsize originalSize = copy? size() : 0;

        *static_cast<memory_helper_iostream*>(this) =
            memory_helper_iostream(newData, newData+newCapacity);

        seek(offset);
        truncate(originalSize);
        return true;
    }

private:
    void init_with_capacity(std::streamsize newCapacity) {
        uint8_t *newData = mAllocator.allocate(newCapacity);
        *static_cast<memory_helper_iostream*>(this) =
            memory_helper_iostream(newData, newData+newCapacity);
        truncate(0);
    }
    Allocator mAllocator;
};


/** Reads & writes to growable blocks on the heap */
template<typename Allocator=std::allocator<uint8_t>>
class heap_iostream : public iostream_base {
public:
    template<typename Allocator2>
    explicit heap_iostream(const Allocator2 &allocator)
        : mAllocator(allocator) {

    }
    heap_iostream() {}


    ~heap_iostream() {
        clear();
    }

    heap_iostream &operator=(heap_iostream &&other) {
        mList = std::move(other.mList);
        mAllocator = std::move(other.mAllocator);
        mCurIndex = other.mCurIndex;
        mCurStream = other.mCurStream;
        mMinBlockSize = other.mMinBlockSize;
        mMaxBlockSize = other.mMaxBlockSize;

        other.mCurIndex     = 0;
        other.mCurStream    = memory_helper_iostream();
        return *this;
    }
    heap_iostream(const heap_iostream&)             =default;
    heap_iostream(heap_iostream&& other) { *this = std::move(other); }



    /** ensures atleast `size` bytes of data is already preallocated. */
    void reserve(std::streamsize size) {
        std::streamsize curCapacity = capacity();

        while(curCapacity < size) {
            std::streamsize remainder = size - curCapacity;
            curCapacity += add_buffer(remainder);
        }
    }

    /** Set the minimum block size to allocate at a time. */
    void set_min_block_size(std::streamsize min) {
        mMinBlockSize = min;
        if(mMinBlockSize > mMaxBlockSize) {
            mMaxBlockSize = mMinBlockSize;
        }
    }

    /** Set the maximum block size to allocate at a time. */
    void set_max_block_size(std::streamsize max) {
        mMaxBlockSize = max;
        if(mMaxBlockSize < mMinBlockSize) {
            mMinBlockSize = mMaxBlockSize;
        }
    }

    /** @return total current capacity. Will grow as needed. */
    std::streamsize capacity() {
        if(mList.size() > 0) {
            mList[mCurIndex] = mCurStream;
        } else {
            return 0;
        }
        std::streamsize totalSize = 0;
        for(memory_helper_iostream &stream : mList) {
            totalSize += stream.capacity();
        }
        return totalSize;
    }

    /** @return current size of the stream. */
    std::streamsize size() {
        if(mList.size() > 0) {
            mList[mCurIndex] = mCurStream;
        } else {
            return 0;
        }
        std::streamsize totalSize = 0;
        for(memory_helper_iostream &stream : mList) {
            totalSize += stream.size();
        }
        return totalSize;
    }

    /** Clears the stream, and truncates it to 0. */
    void clear() {
        for(memory_helper_iostream &stream : mList) {
            mAllocator.deallocate(stream.data(), stream.capacity());
        }
        mList.clear();
        mCurIndex = 0;
        mCurStream = memory_helper_iostream();
    }

    std::streamsize write(const void *data, std::streamsize nBytes) {
        std::streamsize written = mCurStream.write(data, nBytes);
        if(written == nBytes) {
            return nBytes;
        }
        memory_section_istream chunk(data, nBytes);
        chunk.void_skip(written);

        do {
            std::streamsize remainder = chunk.size();
            next(remainder);

            std::streamsize written = mCurStream.write(chunk.cursor(), remainder);
            if(written == remainder) {
                return nBytes;
            }

            chunk.void_skip(written);
        } while(!chunk.eof());
        return chunk.cursor() - reinterpret_cast<const uint8_t*>(data);
    }

    std::streamsize read(void *data, std::streamsize nBytes) {
        if(mList.size() == 0){
            return 0;
        }
        memory_helper_iostream chunk(data, nBytes);

        while(!chunk.eof()) {

            std::streamsize remainder = chunk.capacity() - chunk.tell();
            std::streamsize readBytes = mCurStream.read(chunk.cursor(), remainder);
            chunk.skip_write(readBytes);
            if(mCurStream.eof()) {
                if(!next_read()) {
                    break;
                }
            }
        }
        return chunk.tell();
    }

    std::streamoff tell() const {
        if(mList.size() == 0) {
            return 0;
        }
        std::streamoff offset = 0;
        for(std::size_t i = 0; i < mCurIndex; ++i) {
            offset += mList[i].size();
        }
        offset += mCurStream.tell();
        return offset;
    }

    std::streamoff seek(std::streamoff offset, std::ios_base::seekdir dir = std::ios_base::beg) {
        // if empty nothing to do
        if(mList.size() == 0) {
            return 0;
        }
        switch(dir) {
        default:
        case std::ios_base::beg: break;
        case std::ios_base::cur: offset += tell(); break;
        case std::ios_base::end: offset += size(); break;
        }

        std::streamoff curOff = 0;
        bool found = false;
        std::size_t newIndex = 0;
        for(std::size_t i = 0; i < mList.size(); ++i) {
            memory_helper_iostream &stream = mList[i];
            if(stream.size() == 0) {
                newIndex = i;
                found = true;
                break;
            }
            std::streamoff endOff = stream.size() + curOff;
            if(offset < endOff) {
                newIndex = i;
                found = true;
                break;
            }
            curOff = endOff;
        }
        if(!found && offset >= curOff) {
            newIndex = mList.size()-1;
            curOff -= mList[newIndex].size();
        }


        if(newIndex != mCurIndex) {
            mList[mCurIndex] = mCurStream;
            mCurIndex = newIndex;
            mCurStream = mList[mCurIndex];
        }
        mCurStream.seek(offset - curOff);

        return tell();
    }
private:
    std::streamsize add_buffer(std::streamsize size) {
        size += 4;
        size &= ~3;

        size = clamp(size, mMinBlockSize, mMaxBlockSize);
        bool first = mList.size() == 0;
        mList.push_back(
            memory_helper_iostream(mAllocator.allocate(size), size)
        );
        mList.back().truncate(0);
        if(first) {
            mCurStream = mList[0];
        }
        return size;
    }

    void next(std::streamsize size) {
        if(mList.size() == 0) {
            mCurIndex = 0;
        } else {
            mList[mCurIndex] = mCurStream;
            ++mCurIndex;
        }

        if(mCurIndex == mList.size()) {
            add_buffer(size);
        }

        mCurStream = mList[mCurIndex];
        mCurStream.seek(0);
    }

    bool next_read() {
        if(mCurIndex == mList.size()-1) {
            return false;
        }
        if(mList[mCurIndex+1].size() == 0) {
            return false;
        }
        mList[mCurIndex] = mCurStream;
        ++mCurIndex;
        mCurStream = mList[mCurIndex];
        mCurStream.seek(0);
        return true;
    }
    Allocator mAllocator;
    std::vector<memory_helper_iostream,
        typename std::allocator_traits<Allocator>::template
        rebind_alloc<memory_helper_iostream>> mList;
    std::size_t mCurIndex = 0;
    std::streamsize mMinBlockSize = 1024;
    std::streamsize mMaxBlockSize = 1024*8;
    // more than 2x the spead of this is not a pointer inside mList
    memory_helper_iostream mCurStream;
};

}
