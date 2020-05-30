#pragma once

#include <memory>
#include <optional>

#include "atomic_shared_ptr.h"

namespace LFStructs {

template<typename T>
class LFStack {
    struct Node {
        SharedPtr<Node> next;
        T data;
    };

public:
    LFStack() {}

    void push(const T &data);
    std::optional<T> pop();

private:
    AtomicSharedPtr<Node> top;
};

template<typename T>
void LFStack<T>::push(const T &data) {
    FAST_LOG(Operation::Push, data);
    SharedPtr<Node> newTop(new Node());
    newTop->next = top.get();
    newTop->data = data;
    while (!top.compareExchange(newTop->next.get(), std::move(newTop))) {
        newTop->next = top.get();
    }
}

template<typename T>
std::optional<T> LFStack<T>::pop() {
    FAST_LOG(Operation::Pop, 0);
    FastSharedPtr<Node> res = top.getFast();
    if (res.get() == nullptr)
        return {};

    while (!top.compareExchange(res.get(), res.get()->next.copy())) {
        res = top.getFast();
        if (res.get() == nullptr)
            return {};
    }

    return { res.get()->data };
}

} // namespace LFStructs
