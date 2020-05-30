#pragma once


#include <assert.h>
// streamsize
#include <iosfwd>
#include <ios>
#include <climits>
#include <cstring>

namespace ios {

using std::streamoff;
using std::streamsize;
using std::size_t;

/** @cond PRIVATE **/
template<typename T>
T clamp(T value, T min, T max) {
    if(value < min) {
        return min;
    } else if(value > max) {
        return max;
    }
    return value;
}
/** @endcond */

struct istream_base {
    static constexpr bool is_istream = true;
};

struct ostream_base {
    static constexpr bool is_ostream = true;
};

struct iostream_base : istream_base, ostream_base{

};


/** Checks if T has `std::streamsize read(void*, std::streamsize)` function */
template<typename T>
struct has_read  {
    static constexpr void *ptr = nullptr;

    template <class U>
    static std::true_type testSignature(std::streamsize (U::*)(void*, std::streamsize));

    template<typename U>
    static decltype(testSignature(&U::read)) test(std::nullptr_t);

    template<typename U>
    static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(nullptr))::value;
};



/** Checks if T has `std::streamsize write(const void*, std::streamsize)` function */
template<typename T>
struct has_write  {
    static constexpr void *ptr = nullptr;

    template <class U>
    static std::true_type testSignature(std::streamsize (U::*)(const void*, std::streamsize));

    template<typename U>
    static decltype(testSignature(&U::write)) test(std::nullptr_t);

    template<typename U>
    static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(nullptr))::value;
};

/** Checks if T has `std::streamsize skip(std::streamsize)` function */
template<typename T>
struct has_skip  {
    static constexpr void *ptr = nullptr;

    template <class U>
    static std::true_type testSignature(std::streamsize (U::*)(std::streamsize));

    template<typename U>
    static decltype(testSignature(&U::skip)) test(std::nullptr_t);

    template<typename U>
    static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(nullptr))::value;
};

/** Checks if T has `std::streamoff seek(std::streamoff, std::ios_base::seek_dir)` function */
template<typename T>
struct has_seek  {
    static constexpr void *ptr = nullptr;

    template <class U>
    static std::true_type testSignature(std::streamoff (U::*)(std::streamoff, std::ios_base::seekdir));

    template<typename U>
    static decltype(testSignature(&U::seek)) test(std::nullptr_t);

    template<typename U>
    static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(nullptr))::value;
};



/** Just like /dev/null nothing when read. Accepts everything and discards. */
class null_iostream : public iostream_base {
public:
    null_iostream()                                 = default;
    null_iostream           (const null_iostream&)  = default;
    null_iostream           (null_iostream&&)       = default;
    null_iostream& operator=(const null_iostream&)  = default;
    null_iostream& operator=(null_iostream&&)       = default;

    /* These are so we can use it in a chain stream */
    template<typename T>
    explicit null_iostream(const T&){}
    template<typename T>
    explicit null_iostream(T&&){}

    /** @return 0 always. */
    std::streamsize read(void *buffer, std::streamsize size) {
        (void)buffer; (void)size;
        return 0;
    }
    /** does nothing and returns size like it actually written it fully

        @param buffer This buffer is ignored
        @param size  Will be returned back as if actually written @p size bytes.
        @return always returns @p size
    */
    std::streamsize write(const void*buffer, std::streamsize size) {
        (void)buffer;
        return size;
    }
};

/** Writes directly to stdout */
class bout_ostream {
public:
    bout_ostream();
    std::streamsize write(const void*buffer, std::streamsize size);
    void flush();
private:
    FILE *mFile;
};

/** Reads directly from stdin */
class bin_istream {
public:
    bin_istream();
    std::streamsize read(void *buffer, std::streamsize size);
private:
    FILE *mFile;
};

/** Writes directly to stderr */
class berr_ostream {
public:
    berr_ostream();
    std::streamsize write(const void*buffer, std::streamsize size);

private:
    FILE *mFile;
};

/*  Binary equivalents of cin/cout/cerr. For strings you have 2 options

    1. Use existing cin/cout/cerr as they are pretty much string streams
       already.
    2. Wrap the string stream you want around the binary streams.


*/

/** Binary equivalent to cout */
extern bout_ostream bout;
/** Binary equivalent to cin */
extern bin_istream bin;
/** Binary equivalent to berr */
extern berr_ostream berr;







// source: http://stackoverflow.com/a/4956493/884893
// some processors support byteswapping instructions set.

/** Swaps the endianess of T and returns the result.

    @param u value you want to change endianess of.
    @return returns the swapped endianness.
*/
template <typename T>
T swap_endian(T u)
{
    static_assert (CHAR_BIT == 8, "CHAR_BIT != 8");

    union
    {
        T u;
        unsigned char u8[sizeof(T)];
    } source, dest;

    source.u = u;

    for (size_t k = 0; k < sizeof(T); k++)
        dest.u8[k] = source.u8[sizeof(T) - k - 1];

    return dest.u;
}


/*
    There is no constexpr way to do this because some processors can change
    endianess at runtime.
*/
// source: http://stackoverflow.com/a/4240014/884893
// modified from source
/** @return true if currently executing in little endian mode */
inline bool is_little_endian() {
    short int number = 0x1;
    static_assert(sizeof(number) >= 2, "not enough bits in short to determin endians");
    uint8_t *numPtr = (uint8_t*)&number;
    return (numPtr[0] == 1);
}

/** @return true if currently executing in big endian mode */
inline bool is_big_endian() {
    return !is_little_endian();
}

/** @return true if current executing in endian of network byte order */
inline bool is_network_endian() {
    return is_big_endian();
}

/**
    @param value value assumed to be in native endian.
    @return value in big endian. If currently executing in big endian mode
            does nothing.
*/
template<typename T>
T to_big_endian(T value) {
    return is_big_endian()? value : swap_endian(value);
}

/**
    @param value assumed to be in native endian
    @return value in little endian. If currently executing in little endian mode
            returns original value.
*/
template<typename T>
T to_little_endian(T value) {
    return is_little_endian()? value : swap_endian(value);
}


/** Converts native endian to network byte order.
    @param value assumed to be in native endian
    @return value in network byte order.
*/
template<typename T>
T to_network_endian(T value) {
    return to_big_endian(value);
}

/** Converts value from network byte order to native endian.
    @param value assumed to be in network byte order
    @return value in native endian.
*/
template<typename T>
T from_network_endian(T value) {
    return is_big_endian()? value : swap_endian(value);
}



// -----------------------------------
// Skipping data

/** @cond PRIVATE */
struct skip_skip_tag {};
struct skip_seek_tag {};
struct skip_read_tag {};

/* Each function returns number of bytes skipped */

template<typename InputStream>
std::streamoff skip(InputStream &stream, std::streamoff amount, skip_skip_tag) {
    return stream.skip(amount);
}

template<typename InputStream>
std::streamoff skip(InputStream &stream, std::streamoff amount, skip_seek_tag) {
    std::streamoff curPos = stream.tell();
    return stream.seek(amount, std::ios_base::cur) - curPos;
}

template<typename InputStream>
std::streamoff skip_read(InputStream &stream, std::streamoff skipAmount,
    uint8_t *gargbageStart, uint8_t *gargbageEnd) {
    std::streamsize totalSkipped = 0;

    std::streamsize maxSkipIteration = gargbageEnd - gargbageStart;
    while(totalSkipped < skipAmount) {
        std::streamsize skip = skipAmount - totalSkipped;
        skip = std::min(skip, maxSkipIteration);
        std::streamsize didSkip = stream.read(gargbageStart, skip);
        if(didSkip <= 0) {
            break;
        }
        totalSkipped += didSkip;
    }
    return totalSkipped;
}

struct simple_data_block {
    simple_data_block(std::streamsize size) {
        mData = new uint8_t[size];
        mEnd = mData+size;
    }
    ~simple_data_block() {
        if(mData){
            delete [] mData;
        }
    }

    uint8_t *begin() {
        return mData;
    }
    uint8_t *end() {
        return mEnd;
    }
    uint8_t *mData;
    uint8_t *mEnd;
};

template<typename InputStream>
std::streamoff skip(InputStream &stream, std::streamoff amount, skip_read_tag) {
    simple_data_block garbage(1024*2);
    return skip_read(stream, amount, garbage.begin(), garbage.end());
}



template<bool skip, bool seek, bool read>
struct skip_tag_select_helper {
};

template<bool seek, bool read>
struct skip_tag_select_helper<true, seek, read> {
    typedef skip_skip_tag tag;
};

template<bool read>
struct skip_tag_select_helper<false, true, read> {
    typedef skip_seek_tag tag;
};

template<>
struct skip_tag_select_helper<false, false, true> {
    typedef skip_read_tag tag;
};

template<typename InputStream>
struct skip_tag_select : skip_tag_select_helper<
    has_skip<InputStream>::value,
    has_seek<InputStream>::value,
    has_read<InputStream>::value> {
};

/** @endcond */


template<typename InputStream>
std::streamoff skip(InputStream &stream, std::streamoff amount) {
    typedef typename skip_tag_select<InputStream>::tag tag;
    return skip(stream, amount, tag());
}

/** This struct is used for configuring buffers used when copying streams.

*/
struct copy_config {
    /** initializes everything to nullptr */
    copy_config() {
        buffer_begin    = nullptr;
        buffer_end      = nullptr;
    }

    /**
        @param buffer_begin pointer to start of buffer.
        @param size size of the buffer.
    */
    copy_config(void *buffer_begin, std::streamsize size) {
        this->buffer_begin  = reinterpret_cast<uint8_t*>(buffer_begin);
        this->buffer_end    = this->buffer_begin + size;
    }
    /**
        @param buffer_begin pointer to start of the buffer.
        @param buffer_end pointer to 1 past the last byte of buffer.
    */
    copy_config(void *buffer_begin, void *buffer_end) {
        this->buffer_begin  = reinterpret_cast<uint8_t*>(buffer_begin);
        this->buffer_end    = reinterpret_cast<uint8_t*>(buffer_end);
    }

    /** Set this value to where buffer begins */
    uint8_t *buffer_begin;
    /** Set this value to 1 past the last byte in the buffer */
    uint8_t *buffer_end;
};

/** Writes data until either a write operation returns 0 or less or written
    `size` amount of bytes.

    @param output       the output stream.
    @param begin_ptr    the starting of data to write.
    @param size         size in bytes to write.

    @returns total bytes written.
*/
template<typename Output>
std::streamsize write_fully(Output &output, const void *begin_ptr, std::streamsize size) {
    std::streamsize did_write = output.write(begin_ptr, size);
    if(did_write == size) {
        return size;
    } else if(did_write <= 0) {
        return 0;
    }
    std::streamsize total_write = did_write;
    size -= did_write;
    int iteration = 0;
    const uint8_t *begin    = reinterpret_cast<const uint8_t*>(begin_ptr) + did_write;
    const uint8_t *end      = begin + size;
    while(begin < end) {
        std::streamsize expected_write = end - begin;
        did_write = output.write(begin, expected_write);
        if(did_write <= 0) {
            break;
        }
        total_write += did_write;
        begin += did_write;
        ++iteration;
    }
    return total_write;
}

/** Tries to read the input a full amount of `size` bytes. Stops reading when
    a read operation returns 0 or negative.

    @param input        The input stream.
    @param begin_ptr    ptr to save the data into.
    @param size         size in bytes to read.

    @returns total bytes read.
*/

template<typename Input>
std::streamsize read_fully(Input &input, void *begin_ptr, std::streamsize size) {
    int iteration = 0;
    std::streamsize total_read = 0;

    uint8_t *begin    = reinterpret_cast<uint8_t*>(begin_ptr);
    uint8_t *end      = begin + size;
    while(begin < end) {
        std::streamsize expected_read = end - begin;
        std::streamsize did_read = input.read(begin, expected_read);
        if(did_read <= 0) {
            break;
        }
        total_read += did_read;
        begin += did_read;
        ++iteration;
    }
    return total_read;
}

/** A structure enclosing some details about the copy operation */
struct copy_result {
    /** Total bytes read from input */
    std::streamsize total_read = 0;
    /** Total bytes written to output */
    std::streamsize total_write = 0;
    /** true if did_read == did_write and greator than 0 bytes copied */
    bool success = false;

    explicit operator bool() const {
        return success;
    }
};

/** @cond PRIVATE*/
// private internal class
namespace internal {
    class buffer_guard {
    public:
        ~buffer_guard() {
            if(buffer) {
                delete [] buffer;
            }
        }
        uint8_t *allocate(std::streamsize size) {
            buffer = new uint8_t[size];
            return buffer;
        }
        uint8_t *buffer = nullptr;
    };
}
/** @endcond */

/** Copy data from input to output stream.

    @code
        // use a default heap allocated buffer.
        copy_results results = copy(input, output);
        // if you only care about success/failure
        bool success = copy(input, output).success;

        // example with custom buffer
        std::vector<uint8_t> buffer(2048);
        copy_config config(buffer.data(), buffer.size());
        copy(input, output, config);
    @endcode

    @param input the input stream which will be read from.
    @param output the output stream to copy to
    @param config configuration for copying. See copy_config for details.
           If a buffer is not specified one will be allocated of sensible size.
    @return copy_result detailing how the operation has went.

    @note   copy() is taken in std for copying iterators. copy_stream is good
            name too.
*/
template<typename Input, typename Output>
copy_result copy_stream(Input &input, Output &output, copy_config config = copy_config()) {
    // incase an exception will be thrown
    internal::buffer_guard buffer;

    if(!config.buffer_begin) {
        constexpr std::streamsize size = 2048;
        config.buffer_begin = buffer.allocate(size);
        config.buffer_end   = config.buffer_begin + size;
    }

    std::streamsize size = config.buffer_end - config.buffer_begin;
    copy_result results;

    while(true) {
        std::streamsize did_read = input.read(config.buffer_begin, size);
        if(did_read <= 0) {
            break;
        }
        results.total_read += did_read;
        std::streamsize did_write = write_fully(output, config.buffer_begin, did_read);
        if(did_write > 0) {
            results.total_write += did_write;
        }
        if(did_write < did_read) {
            /// @todo: what should we do? throw exception?
            break;
        }
    }

    if(results.total_read > 0) {
        results.success = results.total_read == results.total_write;
    }

    return results;
}


template<class T, class POD,
    typename std::enable_if<has_read<T>::value>::type*              = nullptr,
    typename std::enable_if<std::is_arithmetic<POD>::value>::type*  = nullptr >
T &operator>>(T &stream, POD &output) {
    bool success = read_fully(stream, &output, sizeof(POD)) == sizeof(POD);
    (void)success;
    /// @todo what to do on failure
    return stream;
}


template<class T, class POD,
    typename std::enable_if<has_write<T>::value>::type*             = nullptr,
    typename std::enable_if<std::is_arithmetic<POD>::value>::type*  = nullptr >
T &operator<<(T &stream, POD input) {
    bool success = write_fully(stream, &input, sizeof(POD)) == sizeof(POD);
    (void)success;
    /// @todo what to do on failure?
    return stream;
}


template<class T,
    typename std::enable_if<has_write<T>::value>::type*     = nullptr>
T &operator<<(T &stream, const char* input) {
    std::streamsize size = std::strlen(input);
    bool success = write_fully(stream, input, size) == size;
    (void)success;
    /// @todo what to do on failure?
    return stream;
}


/*
// Example program

#include "iostream.hpp"
#include <iostream>

int main(){

    FileInputStream inputFile("testFile.txt");

    // allow iterating over text file yay :)
    for(const std::string &line : lineReader(inputFile)) {
        std::cout << line << '\n';
    }
    return 0;
}
*/

}
