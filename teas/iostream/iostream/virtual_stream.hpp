#pragma once

#include <vector>
#include <memory>
#include "ref_stream.hpp"

namespace ios {

/* Naming things is so hard X_X

    name                                Chars
    pmr_istream                         22
    polymorphic_istream_base            24
    polymorphic_istream                 19
    pmr::polymorphic_istream_base       29
    std::pmr::polymorphic_istream_base  34
    poly_istream_base                   17
    istream_base                        12
    pmr_istream_base                    16
    pmr::istream_base                   17
    pmr::pmr_istream_base               21
    pmr_istream                         11
    std::pmr::pmr_istream               21

    pmr_istream it is! It's descriptive. Not too long as I forsee some people
    using it's name alot. And clear as c++17 added pmr = polymorphic namespace
    so people are already getting used to pmr=polymorphic namespace.

    omg we need new name. pmr stands for Polymorphic Memory Resource and not
    PolyMoRphic.
*/

/** Polymorphic istream. */
class p_istream : public istream_base {
public:
    p_istream(){}
    // Because it's virtual, we can't guarantee a safe copy or move.
    // in which case we disable it all.
    p_istream              (p_istream&&)      =delete;
    p_istream &operator=   (p_istream&&)      =delete;
    p_istream              (const p_istream&) =delete;
    p_istream &operator=   (const p_istream&) =delete;

    virtual ~p_istream(){};
    virtual std::streamsize read(void *data, std::streamsize sizeBytes)=0;
};

/*
    Next hard one to name X_X. It's just a thing to wrap an existing istream
    to make it easy to make a pmr_istream out of it. It's ok for this one
    to be longer as it will be less used than pmr_istream.

    example:
        p_istream_wrapper<Stream> *stream = new pmr_istream_wrapper<Stream>(Stream(std::move(prev)));
        p_istream_sub<Stream> *stream = new pmr_istream_sub<Stream>(Stream(std::move(prev)));


    name                                chars
    12345678                            8
    pmr_istream_sub                     18
    pmr_istream_wrapper                 19
    std::pmr::pmr_istream_wrapper       29
    make_pmr_istream                    16
    make_shared                         11 # for comparison :D
    make_istream                        12
    basic_pmr_istream                   17 # doesn't make sense
    pmr_istream_facade                  18 # has negative emotion in english
    pmr_istream_wrap                    16
    pmr_istream_mask                    16
    to_pmr_istream                      14
    pmr_istream_subclass                20
    pmr_istream_sub                     15
    new_pmr_istream                     15

    pmr_istream_sub wins. We can have a to_pmr_istream function too but doesn't
    makes sense as it will return a pointer. maybe a new_pmr_istream so it's
    easier to incorporate an allocator too.
*/

/** Allows easy creation of pmr_istream from an existing istream */
template<typename T>
class p_istream_sub : public p_istream {
public:
    explicit p_istream_sub(T &&stream) : mStream(std::move(stream)) {}
    std::streamsize read(void* data, std::streamsize sizeBytes) override {
        return mStream.read(data, sizeBytes);
    }

    /*  These should not be virtual as it would have problems because it's
        templated.
    */
    T &get_istream() {
        return mStream;
    }

    void set_istream(T &&stream) {
        mStream = std::move(stream);
    }
private:
    T mStream;
};

/** Polymorphic ostream */
class p_ostream : public ostream_base {
public:
    p_ostream(){}
    // Because it's virtual, we can't guarantee a safe copy or move.
    // in which case we disable it all.
    p_ostream              (p_ostream&&)      =delete;
    p_ostream &operator=   (p_ostream&&)      =delete;
    p_ostream              (const p_ostream&) =delete;
    p_ostream &operator=   (const p_ostream&) =delete;

    virtual ~p_ostream(){};
    virtual std::streamsize write(const void *data, std::streamsize sizeBytes)=0;
};

/** Allows you to easily create p_ostream from an existing stream */
template<typename T>
class p_ostream_sub : public p_ostream {
public:
    explicit p_ostream_sub(T &&stream) : mStream(std::move(stream)) {}
    std::streamsize write(const void* data, std::streamsize sizeBytes) override {
        return mStream.write(data, sizeBytes);
    }

    /*  These should not be virtual as it would have problems because it's
        templated.
    */
    T &get_ostream() {
        return mStream;
    }

    void set_ostream(T &&stream) {
        mStream = std::move(stream);
    }
private:
    T mStream;
};




/* This class is meant for flexibility over runtime performance */
/** Allows chaining of streams together.

    Last stream added is the first stream written to.
*/
class chain_istream : public istream_base {
public:
    /** @todo: this needs a better name */
    typedef ptr_istream<p_istream> link_istream;
    ~chain_istream() {
        // order of destruction matters. We must remove the outer layer first
        // so that it will push any buffers down to the inner layer
        for(int i = mChain.size()-1; i >= 0; --i) {
            mChain[i].stream = nullptr;
        }
    }


    template<typename T>
    explicit chain_istream(T &&input) {
        ChainItem item;
        item.stream.reset(new p_istream_sub<T>(std::move(input)));
        mChain.push_back(std::move(item));
    }

    chain_istream           (chain_istream&&)=default;
    chain_istream& operator=(chain_istream&&)=default;

    /** Creates a stream of type Stream initialized with next stream in the
        chain.
    */
    template<typename Stream>
    Stream &push_new() {
        ChainItem item;
        link_istream prev(*(mChain.back().stream));
        p_istream_sub<Stream> *stream = new p_istream_sub<Stream>(Stream(std::move(prev)));
        item.stream.reset(stream);
        mChain.push_back(std::move(item));
        return stream->get_istream();
    }

    /** allocates a new stream of type Stream passing in arguments.

        initializes as `Stream(std::move(prevStream), args...)
    */
    template<typename Stream, typename... Args>
    Stream &push_new(Args... args) {
        ChainItem item;
        link_istream prev(*(mChain.back().stream));
        p_istream_sub<Stream> *stream = new p_istream_sub<Stream>(Stream(std::move(prev), args...));
        item.stream.reset(stream);
        mChain.push_back(std::move(item));
        return stream->get_istream();
    }
    /** Adds stream to chain.

        Stream must support `set_ostream(link_ostream&)`.
    */
    template<typename Stream>
    void push(Stream &&stream) {
        link_istream prev;
        prev.link(*(mChain.back().stream));
        stream.set_istream(prev);

        ChainItem item;
        item.stream = std::unique_ptr<p_istream_sub<Stream>>(std::move(stream));
        mChain.push_back(std::move(item));
    }
    std::streamsize read(void *data, std::streamsize nBytes) {
        return mChain.back().stream->read(data, nBytes);
    }
private:
    class ChainItem {
    public:
        std::unique_ptr<p_istream> stream;
    };
    std::vector<ChainItem> mChain;
};

// This class is meant for flexibility over runtime performance
/** Allows chaining of streams together.

    The last stream added is the first stream that is written to.
*/
class chain_ostream : public ostream_base {
public:
    /** @todo: this needs a better name */
    typedef ptr_ostream<p_ostream> link_ostream;
    ~chain_ostream() {
        // order of destruction matters. We must remove the outer layer first
        // so that it will push any buffers down to the inner layer
        for(int i = mChain.size()-1; i >= 0; --i) {
            mChain[i].stream = nullptr;
        }
    }
    template<typename T>
    explicit chain_ostream(T &&input) {
        ChainItem item;
        item.stream.reset(new p_ostream_sub<T>(std::move(input)));
        mChain.push_back(std::move(item));
    }

    chain_ostream           (chain_ostream&&)=default;
    chain_ostream& operator=(chain_ostream&&)=default;

    template<typename Stream>
    Stream &push_new() {
        ChainItem item;
        link_ostream prev(*(mChain.back().stream));
        p_ostream_sub<Stream> *stream = new p_ostream_sub<Stream>(Stream(std::move(prev)));
        item.stream.reset(stream);
        mChain.push_back(std::move(item));
        return stream->get_ostream();
    }

    /** allocates a new stream of type Stream passing in arguments.

        initializes as `Stream(std::move(prevStream), args...)
    */
    template<typename Stream, typename... Args>
    Stream &push_new(Args... args) {
        ChainItem item;
        link_ostream prev(*(mChain.back().stream));
        p_ostream_sub<Stream> *stream = new p_ostream_sub<Stream>(Stream(std::move(prev), args...));
        item.stream.reset(stream);
        mChain.push_back(std::move(item));
        return stream->get_ostream();
    }

    /** Adds stream to chain.

        Stream must support `set_ostream(link_ostream&)`.
    */
    template<typename Stream>
    void push(Stream &&stream) {
        link_ostream prev;
        prev.link(*(mChain.back().stream));
        stream.set_ostream(prev);

        ChainItem item;
        item.stream = std::unique_ptr<p_ostream_sub<Stream>>(std::move(stream));
        mChain.push_back(std::move(item));
    }
    std::streamsize write(const void *data, std::streamsize nBytes) {
        return mChain.back().stream->write(data, nBytes);
    }
private:
    class ChainItem {
    public:
        std::unique_ptr<p_ostream> stream;
    };
    std::vector<ChainItem> mChain;
};

/** @cond PRIVATE*/
// this class is useless
template<typename T>
class InputStreamHolder {
public:
    InputStreamHolder(){}
    explicit InputStreamHolder(T &&stream) {
        mStream = std::move(stream);
    }

    InputStreamHolder(InputStreamHolder&& other) {
        (*this) = std::move(other);
    }
    InputStreamHolder &operator=(InputStreamHolder&&other) {
        mStream = std::move(other.mStream);
        return *this;
    }
    InputStreamHolder(const InputStreamHolder&)=delete;
    InputStreamHolder &operator=(const InputStreamHolder&)=delete;
    InputStreamHolder &operator=(T&& stream) {
        mStream = std::move(stream);
        return *this;
    }
    std::streamsize read(void *data, std::streamsize nBytes) {
        return mStream.read(data, nBytes);
    }
    // just to make it easier to access specific function of what it's holding
    T *operator->() { return &mStream; }
    T &get() { return mStream; }

private:
    // perhaps optional is better here because it can be invalid.
    // If no optional then T must be able to hold nothing.
    T mStream;
};

/// @todo do the std::if_enabled thing to only allow pod types for this
template<typename T, typename POD>
InputStreamHolder<T> &operator >>(InputStreamHolder<T> &stream, POD &output) {
    stream.read(&output, sizeof(POD));
}
/** @endcond */
}
