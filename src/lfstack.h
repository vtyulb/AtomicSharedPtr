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
    auto node = new Node{};
    node->next = top.get(),
    node->data = data;

    SharedPtr<Node> newTop(node);
    while (!top.compareExchange(node->next.get(), std::move(newTop))) {
        node->next = top.get();
    }
}

template<typename T>
std::optional<T> LFStack<T>::pop() {
    FAST_LOG(Operation::Pop, 0);
    SharedPtr<Node> res = top.get();
    if (res.get() == nullptr)
        return {};

    while (!top.compareExchange(res.get(), res.get()->next.copy())) {
        res = top.get();
        if (res.get() == nullptr)
            return {};
    }

    return { res.get()->data };
}

} // namespace LFStructs
