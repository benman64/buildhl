#include <algorithm>
#include "iostream.hpp"

namespace ios {

// zero_iostream is in it's own file because of the algorithm dependency.

/** Just like /dev/zero, reads 0's and discards what is written. */
class zero_iostream : public iostream_base {
public:
    zero_iostream()                                 = default;
    zero_iostream           (const zero_iostream& ) = default;
    zero_iostream           (      zero_iostream&&) = default;
    zero_iostream& operator=(const zero_iostream& ) = default;
    zero_iostream& operator=(      zero_iostream&&) = default;

    /* These are so we can use it in a chain stream */
    template<typename T>
    explicit zero_iostream(const T&){}
    template<typename T>
    explicit zero_iostream(T&&){}
    /** Fills bufferOut with 0's

        @return `size` always.
    */
    std::streamsize read(void *bufferOut, std::streamsize size) {
        uint8_t* buffer = reinterpret_cast<uint8_t*>(bufferOut);
        std::fill<uint8_t*>(buffer, buffer+size, 0);
        return size;
    }
    /** does nothing and returns size like it actually written it fully */
    std::streamsize write(const void*buffer, std::streamsize size) {
        (void)buffer;
        return size;
    }
};

}
