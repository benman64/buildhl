#pragma once

#include "iostream.hpp"
#include "memory_stream.hpp"
#include "file_stream.hpp"

namespace ios {

template<typename T>
struct has_size : std::is_same<decltype(((T*)0x0)->size()), std::streamsize> {};


/** reads entirety of stream into a vector. Stream must support size().

    Reads only full sizes of Vector::value_type.
*/
template<typename Stream, typename Vector=std::vector<uint8_t>, typename Allocator=typename Vector::allocator_type>
Vector to_vector_helper(Stream &stream, const Allocator &allocator=Allocator()) {
    Vector vector(allocator);
    std::streamsize totalSize = stream.size() - stream.tell();
    std::streamsize count = totalSize/sizeof(typename Vector::value_type);
    std::streamsize countBytes = count*sizeof(typename Vector::value_type);

    vector.resize(count);
    std::streamsize read = stream.read(&vector[0], countBytes);
    if(read < countBytes) {
        vector.resize(read/sizeof(typename Vector::value_type));
    }
    return vector;
}

/** reads entirety of stream into a vector.

    Reads only full sizes of Vector::value_type.　Reads stream to completion.
*/
template<typename Stream, typename Vector=std::vector<uint8_t>, typename Allocator=typename Vector::allocator_type>
Vector to_vector(Stream &stream, const Allocator &allocator=Allocator()) {
    heap_iostream<Allocator> heap(allocator);
    {
        copy_config config;
        std::vector<uint8_t, Allocator> buffer(allocator);
        buffer.resize(1024);
        config.buffer_begin = buffer.data();
        config.buffer_end = buffer.data() + buffer.size();
        copy_stream(stream, heap, config);
    }
    heap.seek(0);
    return to_vector_helper<heap_iostream<Allocator>, Vector, Allocator>(heap, allocator);
}

template<typename Vector=std::vector<uint8_t>, typename Allocator=typename Vector::allocator_type>
Vector read_file(const std::string &fileName, const Allocator &allocator=Allocator()) {
    file_istream inputFile(fileName);
    return to_vector<file_istream, Vector, Allocator>(inputFile, allocator);

}

/** Copies beginIn to endIn into the vector. If vector element size is larger
    than 1 then only full elements are copied.

    @param beginIn      pointer to first element
    @param endIn        pointer to 1 past the last element
    @param allocator    An allocator to initialize the vector with

    @return a vector with specified data copied.
*/
template<typename Vector=std::vector<uint8_t>, typename Allocator=typename Vector::allocator_type>
Vector to_vector(const void *beginIn, const void *endIn, const Allocator& allocator=Allocator()) {
    typedef typename Vector::value_type item;
    const uint8_t *begin = reinterpret_cast<const uint8_t*>(beginIn);
    const uint8_t *end = reinterpret_cast<const uint8_t*>(endIn);
    std::streamsize elementSize = sizeof(Vector::value_type);
    if(elementSize > 1) {
        std::streamsize size = end - begin;
        size /= elementSize;
        size *= elementSize;
        end = begin + size;
    }

    Vector vector(
        reinterpret_cast<const item*>(begin),
        reinterpret_cast<const item*>(end),
        allocator
    );

    return vector;
}


template<class T,
    typename std::enable_if<has_write<T>::value>::type*     = nullptr>
T &operator<<(T &stream, const std::vector<uint8_t> &input) {
    std::streamsize size = input.size()*sizeof(input.data()[0]);
    bool success = stream.write(input.data(), size) == size;
    (void)success;
    /// @todo what to do on failure?
    return stream;
}







// -----------------------------------------------------
// would be in <string>

template <class T, class CharT, class Traits, class Allocator,
    typename std::enable_if<has_write<T>::value>::type* = nullptr>
T& operator<<(T &stream,
    const std::basic_string<CharT, Traits, Allocator>& input) {
    std::streamsize size = input.size()*sizeof(input.data()[0]);
    bool success = stream.write(input.data(), size) == size;
    (void)success;
    /// @todo what to do on failure?
    return stream;


}

}
