#pragma once

#include "iostream.hpp"
#include "ref_stream.hpp"

namespace ios {

/** @cond PRIVATE */
template<typename T>
class iterator_maker {
public:
    typedef typename std::remove_reference<T>::type iterator;

    iterator_maker(const iterator_maker&)   =default;
    iterator_maker(      iterator_maker&&)  =default;
    iterator_maker(T &&first) : mFirst(std::forward<T>(first)) {

    }
    iterator begin() {
        return std::move(mFirst);
    }

    iterator end() {
        return iterator{};
    }
private:

    iterator mFirst;
};

template<typename T, typename ...Args>
iterator_maker<T> enumerate(Args&& ...args) {
    return iterator_maker<T>(T{std::forward<Args>(args)...});
}

template<typename T>
class line_iterator {
public:
    typedef typename std::remove_reference<T>::type TT;
    typedef char char_type;
    line_iterator(const line_iterator&)=default;
    line_iterator(line_iterator&&)=default;
    line_iterator() {

    }
    line_iterator(const std::string &lineEnding) : mLineEnding(lineEnding) {
    }
    line_iterator(T &stream, const std::string &lineEnding) : mStream(&stream), mLineEnding(lineEnding) {
        assert(mStream != nullptr);
        readNextLine();
        mNeedsRead = false;
    }


    const std::string &operator*() {
        if(mNeedsRead){
            readNextLine();
            mNeedsRead = false;
        }
        return mCurLine;
    }

    const std::string *operator->() {
        return &(**this);
    }

    line_iterator &operator++() {
        readNextLine();
        return *this;
    }

    bool operator==(const line_iterator<T> &other) const {
        if(this == &other){
            return true;
        }
        if(other.mStream != mStream) {
            return false;
        }
        if(other.mLineNumber == mLineNumber) {
            return true;
        }
        if(other.mStream == nullptr && mCurLine.empty()) {
            return true;
        }
        return false;
    }

    bool operator!=(const line_iterator<T> &other) const {
        return !((*this) == other);
    }

private:
    void readNextLine() {
        char_type c;
        mCurLine.clear();
        if(mStream == nullptr) {
            return;
        }
        char lastLineChar = mLineEnding[mLineEnding.length()-1];

        do {
            std::streamsize bRead = mStream->read(&c, sizeof(char_type));
            if(bRead <= 0){
                mStream = nullptr;
                break;
            }

            mCurLine += c;

            if(c == lastLineChar) {
                if(mCurLine.substr(mCurLine.length() - mLineEnding.length()) == mLineEnding) {
                    mCurLine.resize(mCurLine.length() - mLineEnding.length());
                    break;
                }
            }

        } while(true);
        ++mLineNumber;
    }
    // iterator does not own the stream
    // can be nullptr in the case of when it's the end
    TT *mStream = nullptr;
    std::string mCurLine;
    bool mNeedsRead = true;
    int mLineNumber = 0;
    std::string mLineEnding;
};

template<typename T>
class line_itext {
public:
    typedef line_iterator<T> iterator;
    line_itext              (line_itext&&)      =default;
    line_itext &operator=   (line_itext&&)      =default;
    line_itext              (const line_itext&) =delete;
    line_itext &operator=   (const line_itext&) =delete;

    /** Takes ownership of stream.

        @param stream the stream to read lines from.
        @param lineEnding line ending to use. Default "\n".
    */
    line_itext(T &&stream, const std::string &lineEnding = "\n") :
        mStream(std::move(stream)), mLineEnding(lineEnding) {
    }
    /** Returns a forward iterator which reads line by line. */
    iterator begin() {
        // Having more than 1 live iterator is UB
        return iterator(mStream, mLineEnding);
    }
    /** End iterator is universal. */
    iterator end() {
        return iterator(mLineEnding);
    }

    /** Reads next line.

        @return next line. The line ending is stripped from returned string.
    */
    std::string read_next() {
        return *begin();
    }

    /* get/set is needed for chaining */

    void set_istream(T &&stream) {
        mStream = std::move(stream);
    }

    /** @return the underlining stream */
    T &get_istream() {
        return mStream;
    }
private:
    T mStream;
    std::string mLineEnding;
};

// class requires <>. As function it can be autodetected and skip <>.
template<typename T>
line_itext<T> lineReader(T &&stream) {
    return line_itext<T>(std::move(stream));
}

template<typename T>
line_itext<ref_istream<T>> make_line_itext(T &stream, const std::string &lineEnding = "\n") {
    return line_itext<ref_istream<T>>(ref_istream<T>(stream), lineEnding);
}
/** @endcond */

}
