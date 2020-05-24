#pragma once

#include <atomic>
#include <cassert>
#include <memory>
#include <thread>

#include "fast_logger.h"

namespace LFStructs {

const int MAGIC_LEN = 16;
const size_t MAGIC_MASK = 0x0000'0000'0000'FFFF;
const int CACHE_LINE_SIZE = 128;

template<typename T>
struct alignas(CACHE_LINE_SIZE) ControlBlock {
    explicit ControlBlock() = delete;
    explicit ControlBlock(T *data)
        : data(data)
        , refCount(1)
    {
        assert(reinterpret_cast<size_t>(data) <= 0x0000'FFFF'FFFF'FFFF);
    }

    T *data;
    std::atomic<size_t> refCount;
};


template<typename T>
class SharedPtr {
public:
    SharedPtr(): controlBlock(nullptr) {}
    explicit SharedPtr(T *data)
        : controlBlock(new ControlBlock<T>(data))
    {
        FAST_LOG(Operation::ObjectCreated, reinterpret_cast<size_t>(controlBlock));
    }
    explicit SharedPtr(ControlBlock<T> *controlBlock): controlBlock(controlBlock) {}
    explicit SharedPtr(const SharedPtr &other) {
        controlBlock = other.controlBlock;
        int before = controlBlock->refCount.fetch_add(1);
        assert(before);
        FAST_LOG(Operation::Ref, (reinterpret_cast<size_t>(controlBlock) << MAGIC_LEN / 2) | before);
    };
    explicit SharedPtr(SharedPtr &&other) { controlBlock = other.controlBlock; other.controlBlock = nullptr; };
    SharedPtr& operator=(const SharedPtr &other) = delete;
    SharedPtr& operator=(SharedPtr &&other) {
        if (controlBlock != other.controlBlock) {
            unref();
            controlBlock = other.controlBlock;
            other.controlBlock = nullptr;
        }
        return *this;
    }
    ~SharedPtr() { unref(); }

    SharedPtr copy() { return SharedPtr(*this); }
    T* get() const { return controlBlock->data; }
    T* operator*() const { return controlBlock->data; }

private:
    void unref() {
        if (controlBlock){
            int before = controlBlock->refCount.fetch_add(-1);
            assert(before);
            FAST_LOG(Operation::Unref, (reinterpret_cast<size_t>(controlBlock) << MAGIC_LEN / 2) | before);
            if (before == 1) {
                FAST_LOG(Operation::ObjectDestroyed, reinterpret_cast<size_t>(controlBlock));
                delete controlBlock->data;
                delete controlBlock;
            }
        }
    }

    template<typename A> friend class AtomicSharedPtr;
    ControlBlock<T> *controlBlock;
};


template<typename T>
class alignas(CACHE_LINE_SIZE) AtomicSharedPtr {
public:
    AtomicSharedPtr(T *data = nullptr);
    ~AtomicSharedPtr();

    AtomicSharedPtr(const AtomicSharedPtr &other) = delete;
    AtomicSharedPtr(AtomicSharedPtr &&other) = delete;
    AtomicSharedPtr& operator=(const AtomicSharedPtr &other) = delete;
    AtomicSharedPtr& operator=(AtomicSharedPtr &&other) = delete;

    SharedPtr<T> get();

    bool compareExchange(T *expected, SharedPtr<T> &&newOne); // this actually is strong version

    void store(T *data);
    void store(SharedPtr<T>&& data);

private:
    void destroyOldControlBlock(size_t oldPackedPtr);

    /* first 48 bit - pointer to control block
     * last 16 bit - local refcount if anyone is accessing control block
     * through current AtomicSharedPtr instance right now */
    std::atomic<size_t> packedPtr;
    static_assert(sizeof(T*) == sizeof(size_t));
};

template<typename T>
AtomicSharedPtr<T>::AtomicSharedPtr(T *data) {
    auto block = new ControlBlock(data);
    packedPtr.store(reinterpret_cast<size_t>(block) << MAGIC_LEN);
}

template<typename T>
SharedPtr<T> AtomicSharedPtr<T>::get() {
    // taking copy and notifying about read in progress
    size_t packedPtrCopy = packedPtr.fetch_add(1);
    FAST_LOG(Operation::Get, packedPtrCopy);
    auto block = reinterpret_cast<ControlBlock<T>*>(packedPtrCopy >> MAGIC_LEN);
    int before = block->refCount.fetch_add(1);
    assert(before);
    // copy is completed

    // notifying about completed copy
    size_t expected = packedPtrCopy + 1;
    while (true) {
        assert((expected & MAGIC_MASK) > 0);
        size_t expCopy = expected;
        if (packedPtr.compare_exchange_weak(expected, expected - 1)) {
            FAST_LOG(Operation::GetRefSucc, expected);
            break;
        }

        // if control block pointer just changed, then
        // handling object's refcount is not our responsibility
        if (((expected >> MAGIC_LEN) != (packedPtrCopy >> MAGIC_LEN)) ||
                ((expected & MAGIC_MASK) == 0)) // >20 hours wasted here
        {
            int before = block->refCount.fetch_sub(1);
            assert(before);
            FAST_LOG(Operation::Unref, before);
            FAST_LOG(Operation::GetRefAbrt, packedPtrCopy);
            break;
        }

        if ((expected & MAGIC_MASK) == 0) {
            abort();
            break;
        }
    }
    // notification finished

    return SharedPtr<T>(block);
}

template<typename T>
AtomicSharedPtr<T>::~AtomicSharedPtr() {
    auto block = reinterpret_cast<ControlBlock<T>*>(packedPtr.load() >> MAGIC_LEN);
    assert((packedPtr & MAGIC_MASK) == 0);
    destroyOldControlBlock(packedPtr);
}

template<typename T>
void AtomicSharedPtr<T>::store(T *data) {
    store(SharedPtr<T>(data));
}

template<typename T>
void AtomicSharedPtr<T>::store(SharedPtr<T> &&data) {
    auto oldPackedPtr = packedPtr.exchange(reinterpret_cast<size_t>(data.controlBlock) << MAGIC_LEN);
    data.controlBlock = nullptr;
    destroyOldControlBlock(oldPackedPtr);
}

template<typename T>
bool AtomicSharedPtr<T>::compareExchange(T *expected, SharedPtr<T> &&newOne) {
    auto holder = this->get();
    FAST_LOG(Operation::CompareAndSwap, reinterpret_cast<size_t>(holder.controlBlock));
    if (holder.get() == expected) {
        size_t holdedPtr = reinterpret_cast<size_t>(holder.controlBlock);
        size_t desiredPackedPtr = reinterpret_cast<size_t>(newOne.controlBlock) << MAGIC_LEN;
        size_t expectedPackedPtr = holdedPtr << MAGIC_LEN;
        while (holdedPtr == (expectedPackedPtr >> MAGIC_LEN)) {
            if (expectedPackedPtr & MAGIC_MASK) {
                int diff = expectedPackedPtr & MAGIC_MASK;
                holder.controlBlock->refCount.fetch_add(diff);
                if (!packedPtr.compare_exchange_weak(expectedPackedPtr, expectedPackedPtr & ~MAGIC_MASK)) {
                    holder.controlBlock->refCount.fetch_sub(diff);
                }
                continue;
            }
            assert((expectedPackedPtr >> MAGIC_LEN) != (desiredPackedPtr >> MAGIC_LEN));
            if (packedPtr.compare_exchange_weak(expectedPackedPtr, desiredPackedPtr)) {
                FAST_LOG(Operation::GetInCAS, expectedPackedPtr);
                newOne.controlBlock = nullptr;
                assert((expectedPackedPtr >> MAGIC_LEN) == holdedPtr);
                destroyOldControlBlock(expectedPackedPtr);
                return true;
            }
        }
    }

    FAST_LOG(Operation::CASAbrt, reinterpret_cast<size_t>(holder.get()));
    return false;
}

template<typename T>
void AtomicSharedPtr<T>::destroyOldControlBlock(size_t oldPackedPtr) {
    FAST_LOG(Operation::CASDestructed, oldPackedPtr);
    size_t localRefcount = (oldPackedPtr & MAGIC_MASK);
    int diff = localRefcount - 1;
    if (diff != 0) {
        auto block = reinterpret_cast<ControlBlock<T>*>(oldPackedPtr >> MAGIC_LEN);
        auto refCountBefore = block->refCount.fetch_add(diff);
        FAST_LOG(Operation::Unref, refCountBefore);
        assert(refCountBefore);
        if (refCountBefore == -diff) {
            FAST_LOG(Operation::ObjectDestroyed, reinterpret_cast<size_t>(block));
            delete block->data;
            delete block;
        }
    }
    FAST_LOG(Operation::CASFin, oldPackedPtr);
}

} // namespace LFStructs
