#pragma once

#include <assert.h>

#include <iosfwd>
#include <memory>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <vector>

#include "memory_stream.hpp"

namespace ios {
// async

/** @cond PRIVATE */
// InputStream must be buffered to be useful, otherwise it doesn't make sense.
// mutex is not movable so we need an implemention that will be allocated
// and wrapped inside a unique_ptr.
template<typename T>
class AsyncInputStream_impl {
    typedef std::thread thread;
    typedef std::mutex mutex;
    typedef std::condition_variable condition_variable;
    typedef memory_iostream<> memory_block;

public:
    AsyncInputStream_impl               (AsyncInputStream_impl&&)       = delete;
    AsyncInputStream_impl &operator=    (AsyncInputStream_impl&& other) = delete;
    AsyncInputStream_impl               (const AsyncInputStream_impl&)  = delete;
    AsyncInputStream_impl &operator=    (const AsyncInputStream_impl&)  = delete;

    AsyncInputStream_impl(std::streamsize blockSize = 20148, std::streamsize maxBlocks = 16) {
        mBlockSize  = blockSize;
        mMaxBlocks  = maxBlocks;
        mRunning    = false;
        mFinished   = false;

        mBlocks.reserve(mMaxBlocks);
    }

    AsyncInputStream_impl(T &&stream, std::streamsize blockSize = 2048,
        std::streamsize maxBlocks = 16) : mStream(std::move(stream)) {
        set_block_size(blockSize, maxBlocks);
        mRunning    = false;
        mFinished   = false;

        mBlocks.reserve(mMaxBlocks);
    }

    ~AsyncInputStream_impl() {
        quitThread();
    }

    void set_block_size(std::streamsize blockSize, std::streamsize maxBlocks) {
        mBlockSize  = blockSize;
        mMaxBlocks  = maxBlocks;
        mBlocks.reserve(mMaxBlocks);
    }

    std::streamsize read(void* data, std::streamsize size) {
        /** @todo: some people will not realize start_if_needed() or try to hack
                  by doing a read(nullptr, 0) as a higher level class might
                  abstract away this interface. Should this be allowed?
        */
        start_if_needed();
        if(size <= 0) {
            return 0;
        }
        memory_helper_iostream block(data, size);

        std::unique_lock<mutex> lock(mMutex);
        mCondition.wait(lock, [&] () -> bool {
            return !mRunning || !mBlocks.empty();
        });
        lock.unlock();
        std::streamsize totalRead = 0;
        while(true) {
            if(mBlocks.empty()){
                break;
            }
            assert(mBlocks[0].size() != mBlocks[0].tell());
            std::streamsize bRead = mBlocks[0].read(block.cursor(), block.capacity() - block.tell());
            if(mBlocks[0].size() == mBlocks[0].tell()) {
                lock.lock();
                mBlocks.erase(mBlocks.begin());
                mCondition.notify_all();
                lock.unlock();
            }
            if(bRead > 0) {
                block.seek(bRead, std::ios_base::cur);
                totalRead += bRead;
            }
            if(block.tell() == block.capacity()) {
                break;
            }
        }
        return totalRead;
    }

    void start_if_needed() {
        if(mRunning || mFinished) {
            return;
        }
        mRunning = true;
        mThread = thread([this]{ this->readerThread(); });
    }

    /* We need get/set to allow chaining for filter */
    void set_istream(T &&stream) {
        mStream = std::move(stream);
    }

    T &get_istream() {
        return mStream;
    }
private:
    /** @returns true if block is full */
    std::streamsize readIntoBlock(memory_block &block) {
        std::streamsize remaining = block.capacity() - block.tell();
        assert(remaining != 0);
        if(remaining == 0){
            return 0;
        }
        std::streamsize bRead = mStream.read(block.cursor(), remaining);
        if(bRead > 0){
            block.skip_write(bRead);
        }
        return bRead;
    }
    void readerThread() {
        memory_block curBlock(mBlockSize);
        while(true) {
            int numBlocks = 0;
            {
                std::unique_lock<mutex> lock(mMutex);
                mCondition.wait(lock, [&] () -> bool {
                    return !mRunning || canReadMore();
                });
                if(!mRunning) {
                    if(curBlock.tell() > 0){
                        curBlock.seek(0);
                        mBlocks.push_back(std::move(curBlock));
                    }
                    break;
                }
                numBlocks = mBlocks.size();
            }
            std::streamsize bRead = readIntoBlock(curBlock);
            bool isFull = curBlock.tell() == curBlock.capacity();
            bool hasData = curBlock.tell() > 0;
            if(hasData && (isFull || numBlocks == 0 || bRead <= 0)) {
                curBlock.truncate(curBlock.tell());
                {
                    std::unique_lock<mutex> lock(mMutex);
                    curBlock.seek(0);
                    mBlocks.push_back(std::move(curBlock));
                    mCondition.notify_all();
                }
                curBlock.set_capacity(mBlockSize);
                curBlock.truncate(0);
            }
            if(bRead == 0) {
                std::unique_lock<mutex> lock(mMutex);
                mRunning = false;
                mFinished = true;
                mCondition.notify_all();
                break;
            }
        }

    }
    bool canReadMore() {
        return static_cast<std::streamsize>(mBlocks.size()) < mMaxBlocks;
    }

    void quitThread() {
        {
            std::unique_lock<mutex> lock(mMutex);
            mRunning = false;
            mCondition.notify_all();
        }
        if(mThread.joinable()){
            mThread.join();
        }
    }

    T                           mStream;
    thread                      mThread;
    mutex                       mMutex;
    condition_variable          mCondition;

    std::vector<memory_block>   mBlocks;
    std::streamsize             mBlockSize;
    std::streamsize             mMaxBlocks;
    bool                        mRunning;
    bool                        mFinished;
};
/** @endcond */

/** Creates a new thread and reads ahead so when you do a read it will be
    quicker.
*/
template<typename T>
class async_istream {
    typedef AsyncInputStream_impl<T> PrivateImp;
public:
    async_istream            (async_istream&&)        =default;
    async_istream &operator= (async_istream&&)        =default;
    async_istream            (const async_istream&)   =delete;
    async_istream &operator= (const async_istream)    =delete;

    /** You must explicitly call start_if_needed, or on first read() the read
        ahead thread will be spawned.
    */
    async_istream(std::streamsize blockSize = 2048,
        std::streamsize maxBlocks = 16) {
        mImpl.reset(new PrivateImp(blockSize, maxBlocks));
    }

    /** Emediatly starts thread reading the stream */
    async_istream(T &&stream, std::streamsize blockSize = 2048,
        std::streamsize maxBlocks = 16) {
        mImpl.reset(new PrivateImp(std::move(stream), blockSize, maxBlocks));
        start_if_needed();
    }

    std::streamsize read(void* data, std::streamsize size) {
        return mImpl->read(data, size);
    }

    /* get/set is needed for chaining */

    void set_istream(T &&stream) {
        mImpl->set_istream(std::move(stream));
    }

    T &get_istream() {
        return mImpl->get_istream();
    }

    void start_if_needed() {
        mImpl->start_if_needed();
    }

private:
    std::unique_ptr<PrivateImp> mImpl;
};

/** @cond PRIVATE */
template<typename T>
class async_ostream_impl {
    typedef std::thread thread;
    typedef std::mutex mutex;
    typedef std::condition_variable condition_variable;
    typedef memory_iostream<> memory_block;

public:
    async_ostream_impl              (async_ostream_impl&&)          = delete;
    async_ostream_impl &operator=   (async_ostream_impl&& other)    = delete;
    async_ostream_impl              (const async_ostream_impl&)     = delete;
    async_ostream_impl &operator=   (const async_ostream_impl&)     = delete;

    async_ostream_impl() {
        mBlockSize  = 2048;
        mMaxBlocks  = 16;
        mRunning    = false;
        mFinished   = false;

        mBlocks.reserve(mMaxBlocks);
        init_cur_block();
    }

    async_ostream_impl(T &&stream, std::streamsize blockSize = 2048,
        std::streamsize maxBlocks = 16) : mStream(std::move(stream)) {
        set_block_size(blockSize, maxBlocks);
        mRunning    = false;
        mFinished   = false;

        mBlocks.reserve(mMaxBlocks);
        init_cur_block();
    }

    ~async_ostream_impl() {
        quit_thread();
    }

    void set_block_size(std::streamsize blockSize, std::streamsize maxBlocks) {
        mBlockSize  = blockSize;
        mMaxBlocks  = maxBlocks;
        mBlocks.reserve(mMaxBlocks);
    }

    std::streamsize write(const void* data, std::streamsize size) {
        std::streamsize bWrite = mCurBlock.write(data, size);
        if(bWrite == size) {
            if(mCurBlock.size() == mCurBlock.capacity()) {
                start_if_needed();
                std::unique_lock<mutex> lock(mMutex);
                push_back_cur_block(lock);
            }
            return size;
        }

        /** @todo: some people will not realize start_if_needed() or try to hack
                  by doing a read(nullptr, 0) as a higher level class might
                  abstract away this interface. Should this be allowed?
        */
        start_if_needed();
        if(size <= 0) {
            return 0;
        }
        memory_section_istream block(data, size);
        block.void_skip(bWrite);

        std::unique_lock<mutex> lock(mMutex);
        do {
            push_back_cur_block(lock);
            std::streamsize nextSize = block.size();
            bWrite = mCurBlock.write(block.cursor(), nextSize);
            if(bWrite == nextSize) {
                if(mCurBlock.size() == mCurBlock.capacity()) {
                    push_back_cur_block(lock);
                }
                break;
            } else {
                block.void_skip(bWrite);
            }
        } while(!block.eof());
        mCondition.wait(lock, [&] () -> bool {
            return !mRunning || can_write_more();
        });
        lock.unlock();
        return size;
    }

    /** start thread now if needed */
    void start_if_needed() {
        if(mRunning || mFinished) {
            return;
        }
        mRunning = true;
        mThread = thread([this]{ this->write_thread(); });
    }

    /* We need get/set to allow chaining for filter */
    void set_ostream(T &&stream) {
        mStream = std::move(stream);
    }

    T &get_ostream() {
        return mStream;
    }

    /** Waits until everything has been fully written to stream.

        @return true if successfully written. false if there was a failure.
    */
    bool flush() {
        std::unique_lock<mutex> lock(mMutex);
        push_back_cur_block(lock);
        mCondition.wait(lock, [&] () -> bool {
            return !mRunning || mBlocks.size() == 0;
        });
        return mBlocks.size() == 0;
    }
private:

    void write_thread() {
        std::unique_lock<mutex> lock(mMutex);
        while(true) {
            mCondition.wait(lock, [&] () -> bool {
                return !mRunning || mBlocks.size() > 0;
            });
            if(!mRunning) {
                break;
            }
            memory_block block = std::move(mBlocks[0]);
            mBlocks.erase(mBlocks.begin());
            mCondition.notify_all();
            lock.unlock();
            std::streamsize written = write_fully(mStream,
                block.data(), block.size());
            if(written != block.size()) {
                mWriteFull = true;
                mRunning = false;
                break;
            }
            lock.lock();
        }
    }

    bool can_write_more() {
        return static_cast<std::streamsize>(mBlocks.size()) < mMaxBlocks;
    }

    /** flushes and signals thread to quit, blocks until thread is done. */
    void quit_thread() {
        flush();
        {
            std::unique_lock<mutex> lock(mMutex);
            mRunning = false;
            mCondition.notify_all();
        }
        if(mThread.joinable()){
            mThread.join();
        }
    }

    void push_back_cur_block(std::unique_lock<mutex> &lock) {
        if(mCurBlock.size() == 0) {
            return;
        }
        mCondition.wait(lock, [&] () -> bool {
            return !mRunning || can_write_more();
        });
        mBlocks.push_back(std::move(mCurBlock));
        mCurBlock = memory_block(mBlockSize);
        mCondition.notify_all();
    }

    void init_cur_block() {
        mCurBlock = memory_block(mBlockSize);
    }

    T                           mStream;
    thread                      mThread;
    mutex                       mMutex;
    condition_variable          mCondition;

    std::vector<memory_block>   mBlocks;
    memory_block                mCurBlock;
    std::streamsize             mBlockSize;
    std::streamsize             mMaxBlocks;
    bool                        mRunning;
    bool                        mFinished;
    bool                        mWriteFull = false;
};

/** @endcond */

/** Queues writes onto a separate thread. Making write operations non-blocking.
*/
template<typename T>
class async_ostream {
    typedef async_ostream_impl<T> PrivateImp;
public:
    async_ostream            (async_ostream&&)        =default;
    async_ostream &operator= (async_ostream&&)        =default;
    async_ostream            (const async_ostream&)   =delete;
    async_ostream &operator= (const async_ostream)    =delete;

    /** You must explicitly call start_if_needed, or on first read() the read
        ahead thread will be spawned.
    */
    async_ostream(std::streamsize blockSize = 2048,
        std::streamsize maxBlocks = 16) {
        mImpl.reset(new PrivateImp(blockSize, maxBlocks));
    }

    /** Emediatly starts thread ready for writing stream */
    async_ostream(T &&stream, std::streamsize blockSize = 2048,
        std::streamsize maxBlocks = 16) {
        mImpl.reset(new PrivateImp(std::move(stream), blockSize, maxBlocks));
        start_if_needed();
    }

    std::streamsize write(const void* data, std::streamsize size) {
        return mImpl->write(data, size);
    }

    /* get/set is needed for chaining */

    void set_ostream(T &&stream) {
        mImpl->set_ostream(std::move(stream));
    }

    T &get_ostream() {
        return mImpl->get_ostream();
    }

    /** Starts background thread now if it has not been started yet */
    void start_if_needed() {
        mImpl->start_if_needed();
    }

    /** Waits until everything has been fully written to stream.

        @return true if successfully written. false if there was a failure.
    */
    bool flush() {
        return mImpl->flush();
    }

private:
    std::unique_ptr<PrivateImp> mImpl;
};

}
