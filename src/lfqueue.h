#pragma once

#include "atomic_shared_ptr.h"

namespace LFStructs {

template<typename T>
class LFQueue {
    struct Node {
        AtomicSharedPtr<Node> next;
        T data;
        std::atomic_flag consumed;
    };

public:
    LFQueue();

    void push(const T &data);
    std::optional<T> pop();

private:
    AtomicSharedPtr<Node> front;
    AtomicSharedPtr<Node> back;
};

template<typename T>
LFQueue<T>::LFQueue() {
    auto fakeNode = SharedPtr(new Node{
                                  .consumed = true
                              });

    front.compareExchange(nullptr, fakeNode.copy());
    back.compareExchange(nullptr, std::move(fakeNode));
}

template<typename T>
void LFQueue<T>::push(const T &data) {
    FAST_LOG(Operation::Push, data);
    auto newBack = SharedPtr(new Node{
                                 .data = data
                             });

    while (1) {
        SharedPtr<Node> currentBack = back.get();
        if (currentBack.get()->next.compareExchange(nullptr, newBack.copy())) {
            back.compareExchange(currentBack.get(), std::move(newBack));
            return;
        } else {
            SharedPtr<Node> realPtr = currentBack.get()->next.get();
            assert(realPtr.get() != nullptr);
            back.compareExchange(currentBack.get(), std::move(realPtr));
        }
    }
}

template<typename T>
std::optional<T> LFQueue<T>::pop() {
    FAST_LOG(Operation::Pop, 0);
    auto res = front.get();
    while (res.get()->consumed.test_and_set()) {
        auto nextPtr = res.get()->next.get();
        if (nextPtr.get() == nullptr) {
            return {};
        }
        front.compareExchange(res.get(), nextPtr.copy());
        res = front.get();
    }

    return { res.get()->data };
}

} // namespace LFStructs
