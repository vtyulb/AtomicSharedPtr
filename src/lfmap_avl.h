#pragma once

#include <utility>

#include "atomic_shared_ptr.h"

namespace LFStructs {

template<typename Key, typename Value>
class LFMapAvl {
    struct Node {
        SharedPtr<Node> left;
        SharedPtr<Node> right;

        Key key;
        Value data;
        int height;

        void updateHeight() {
            height = std::max(LFMapAvl::height(left), LFMapAvl::height(right)) + 1;
        }
    };

public:
    LFMapAvl() = default;

    void upsert(Key key, Value data);
    std::optional<Value> get(Key key);
    void remove(Key key);

private:
    static int height(const SharedPtr<Node> &node);

    SharedPtr<Node> rotateLeft(const SharedPtr<Node> &root);
    SharedPtr<Node> rotateRight(const SharedPtr<Node> &root);
    SharedPtr<Node> bigRotateLeft(const SharedPtr<Node> &root);
    SharedPtr<Node> bigRotateRight(const SharedPtr<Node> &root);

    SharedPtr<Node> upsert(const SharedPtr<Node> &root, Key key, Value data);
    SharedPtr<Node> remove(const SharedPtr<Node> &root, Key key);

    SharedPtr<Node> balance(const SharedPtr<Node> &root);

    AtomicSharedPtr<Node> treeRoot;
};

template<typename Key, typename Value>
int LFMapAvl<Key, Value>::height(const SharedPtr<LFMapAvl::Node> &node) {
    if (node.get() == nullptr)
        return 0;
    else
        return node.get()->height;
}

template<typename Key, typename Value>
void LFMapAvl<Key, Value>::upsert(Key key, Value data) {
    while (true) {
        auto root = treeRoot.get();
        auto newRoot = upsert(root, key, data);
        if (treeRoot.compareExchange(root.get(), std::move(newRoot)))
            break;
    }
}

template<typename Key, typename Value>
void LFMapAvl<Key, Value>::remove(Key key) {
    while (true) {
        auto root = treeRoot.get();
        auto newRoot = remove(root, key);
        if (treeRoot.compareExchange(root.get(), std::move(newRoot)))
            return;
    }
}

template<typename Key, typename Value>
std::optional<Value> LFMapAvl<Key, Value>::get(Key key) {
    auto holder = treeRoot.getFast();
    Node *root = holder.get();
    while (root != nullptr) {
        assert(abs(height(root->left) - height(root->right)) < 2);
        if (root->key == key)
            return root->data;
        else if (root->key < key)
            root = root->right.get();
        else
            root = root->left.get();
    }

    return {};
}

template<typename Key, typename Value>
SharedPtr<typename LFMapAvl<Key, Value>::Node> LFMapAvl<Key, Value>::upsert(const SharedPtr<Node> &root, Key key, Value data) {
    if (root.get() == nullptr) {
        SharedPtr<Node> res(new Node());
        res->key = key;
        res->data = data;
        res->height = 1;
        return res;
    }

    SharedPtr<Node> newRoot(new Node());
    newRoot->key = root->key;
    newRoot->data = root->data;

    if (root->key == key) {
        newRoot->left = root->left;
        newRoot->right = root->right;
        newRoot->data = data;
        newRoot->height = root->height;
        return newRoot;
    } else if (root->key < key) {
        newRoot->left = root->left;
        newRoot->right = upsert(root->right, key, data);
        newRoot->updateHeight();
        return balance(newRoot);
    } else {
        newRoot->left = upsert(root->left, key, data);
        newRoot->right = root->right;
        newRoot->updateHeight();
        return balance(newRoot);
    }
}

template<typename Key, typename Value>
SharedPtr<typename LFMapAvl<Key, Value>::Node> LFMapAvl<Key, Value>::balance(const SharedPtr<Node> &root) {
    int diff = height(root->left) - height(root->right);
    if (abs(diff) < 2)
        return root;

    assert(abs(diff) < 3);

    if (diff == 2) {
        if (height(root->left->right) <= height(root->left->left))
            return rotateRight(root);
        else
            return bigRotateRight(root);
    } else {
        if (height(root->right->left) <= height(root->right->right))
            return rotateLeft(root);
        else
            return bigRotateLeft(root);
    }
}

template<typename Key, typename Value>
SharedPtr<typename LFMapAvl<Key, Value>::Node> LFMapAvl<Key, Value>::rotateLeft(const SharedPtr<Node> &root) {
    SharedPtr<Node> a(new Node());
    a->key = root->key;
    a->data = root->data;
    a->left = root->left;
    a->right = root->right->left;
    a->updateHeight();

    SharedPtr<Node> b(new Node());
    b->key = root->right->key;
    b->data = root->right->data;
    b->left = std::move(a);
    b->right = root->right->right;
    b->updateHeight();

    return b;
}

template<typename Key, typename Value>
SharedPtr<typename LFMapAvl<Key, Value>::Node> LFMapAvl<Key, Value>::rotateRight(const SharedPtr<Node> &root) {
    SharedPtr<Node> a(new Node());
    a->key = root->key;
    a->data = root->data;
    a->left = root->left->right;
    a->right = root->right;
    a->updateHeight();

    SharedPtr<Node> b(new Node());
    b->key = root->left->key;
    b->data = root->left->data;
    b->left = root->left->left;
    b->right = std::move(a);
    b->updateHeight();

    return b;
}

template<typename Key, typename Value>
SharedPtr<typename LFMapAvl<Key, Value>::Node> LFMapAvl<Key, Value>::bigRotateLeft(const SharedPtr<Node> &root) {
    SharedPtr<Node> a(new Node());
    a->key = root->key;
    a->data = root->data;
    a->left = root->left;
    a->right = root->right->left->left;
    a->updateHeight();

    SharedPtr<Node> b(new Node());
    b->key = root->right->key;
    b->data = root->right->data;
    b->left = root->right->left->right;
    b->right = root->right->right;
    b->updateHeight();

    SharedPtr<Node> c(new Node());
    c->key = root->right->left->key;
    c->data = root->right->left->data;
    c->left = std::move(a);
    c->right = std::move(b);
    c->updateHeight();

    return c;
}

template<typename Key, typename Value>
SharedPtr<typename LFMapAvl<Key, Value>::Node> LFMapAvl<Key, Value>::bigRotateRight(const SharedPtr<Node> &root) {
    SharedPtr<Node> a(new Node());
    a->key = root->key;
    a->data = root->data;
    a->left = root->left->right->right;
    a->right = root->right;
    a->updateHeight();

    SharedPtr<Node> b(new Node());
    b->key = root->left->key;
    b->data = root->left->data;
    b->left = root->left->left;
    b->right = root->left->right->left;
    b->updateHeight();

    SharedPtr<Node> c(new Node());
    c->key = root->left->right->key;
    c->data = root->left->right->data;
    c->left = std::move(b);
    c->right = std::move(a);
    c->updateHeight();

    return c;
}

template<typename Key, typename Value>
SharedPtr<typename LFMapAvl<Key, Value>::Node> LFMapAvl<Key, Value>::remove(const SharedPtr<Node> &root, Key key) {
    if (root.get() == nullptr)
        return root;

    if (root->key < key) {
        auto newRight = remove(root->right, key);
        if (newRight.get() == root->right.get())
            return root;

        SharedPtr<Node> newRoot(new Node());
        newRoot->key = root->key;
        newRoot->data = root->data;
        newRoot->left = root->left;
        newRoot->right = std::move(newRight);
        newRoot->updateHeight();

        return balance(newRoot);
    } else if (root->key > key) {
        auto newLeft = remove(root->left, key);
        if (newLeft.get() == root->left.get())
            return root;

        SharedPtr<Node> newRoot(new Node());
        newRoot->key = root->key;
        newRoot->data = root->data;
        newRoot->left = std::move(newLeft);
        newRoot->right = root->right;
        newRoot->updateHeight();

        return balance(newRoot);
    } else {
        Node *targetLeft = root->left.get();
        Node *targetRight = root->right.get();
        if (targetLeft == nullptr && targetRight == nullptr)
            return {};

        if (targetRight == nullptr || (targetLeft && targetLeft->height > targetRight->height)) {
            while (targetLeft->right.get() != nullptr)
                targetLeft = targetLeft->right.get();

            SharedPtr<Node> newRoot(new Node());
            newRoot->key = targetLeft->key;
            newRoot->data = targetLeft->data;
            newRoot->left = remove(root->left, targetLeft->key);
            newRoot->right = root->right;
            newRoot->updateHeight();

            return balance(newRoot);
        } else {
            while (targetRight->left.get() != nullptr)
                targetRight = targetRight->left.get();

            SharedPtr<Node> newRoot(new Node());
            newRoot->key = targetRight->key;
            newRoot->data = targetRight->data;
            newRoot->left = root->left;
            newRoot->right = remove(root->right, targetRight->key);
            newRoot->updateHeight();

            return balance(newRoot);
        }
    }
}

} // namespace LFStructs
