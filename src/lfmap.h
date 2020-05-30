#pragma once

namespace LFStructs {

template<typename Key, typename Value>
class LFMap {
    struct Node {
        SharedPtr<Node> left;
        SharedPtr<Node> right;

        Key key;
        Value data;
        int size;

        void updateSize() {
            size = (left.get() == nullptr ? 0 : left->size) + (right.get() == nullptr ? 0 : right->size) + 1;
        }
    };

public:
    LFMap() = default;

    void upsert(Key key, Value value);
    std::optional<Value> get(Key key);
    void remove(Key key);

private:
    static std::pair<SharedPtr<Node>, SharedPtr<Node>> splitLess(const SharedPtr<Node> &root, Key key);
    static std::pair<SharedPtr<Node>, SharedPtr<Node>> splitLessEq(const SharedPtr<Node> &root, Key key);
    SharedPtr<Node> merge(const SharedPtr<Node> &left, const SharedPtr<Node> &right);

    AtomicSharedPtr<Node> root;
};

template<typename Key, typename Value>
std::optional<Value> LFMap<Key, Value>::get(Key key) {
    FastSharedPtr<Node> rootCopy = root.getFast();
    Node *node = rootCopy.get();
    while (node != nullptr) {
        if (node->key < key)
            node = node->right.get();
        else if (node->key > key)
            node = node->left.get();
        else
            return node->data;
    }

    return {};
}

template<typename Key, typename Value>
void LFMap<Key, Value>::upsert(Key key, Value value) {
    SharedPtr node(new Node());
    node->key = key;
    node->data = value;
    node->size = 1;

    while (true) {
        SharedPtr<Node> rootCopy = root.get();
        auto [left, right] = splitLess(rootCopy, key);
        auto [rightLeft, rightRight] = splitLessEq(right, key);

        SharedPtr<Node> newRoot = merge(left, merge(node, rightRight));
        if (root.compareExchange(rootCopy.get(), std::move(newRoot)))
            return;
    }
}

template<typename Key, typename Value>
void LFMap<Key, Value>::remove(Key key) {
    while (true) {
        SharedPtr<Node> rootCopy = root.get();
        auto [left, right] = splitLess(rootCopy, key);
        auto [rightLeft, rightRight] = splitLessEq(right, key);

        SharedPtr<Node> newRoot = merge(left, rightRight);
        if (root.compareExchange(rootCopy.get(), std::move(newRoot)))
            return;
    }
}

template<typename Key, typename Value>
SharedPtr<typename LFMap<Key, Value>::Node> LFMap<Key, Value>::merge(const SharedPtr<Node> &left, const SharedPtr<Node> &right) {
    if (left.get() == nullptr)
        return right;
    if (right.get() == nullptr)
        return left;

    SharedPtr<Node> root(new Node());
    root->size = left->size + right->size;
    if (rand() * uint64_t(left->size + right->size) < left->size * RAND_MAX) {
        root->key = left->key;
        root->data = left->data;
        root->left = left->left;
        root->right = merge(left->right, right);
    } else {
        root->key = right->key;
        root->data = right->data;
        root->left = merge(left, right->left);
        root->right = right->right;
    }

    return root;
}

template<typename Key, typename Value>
std::pair<SharedPtr<typename LFMap<Key, Value>::Node>, SharedPtr<typename LFMap<Key, Value>::Node>>
LFMap<Key, Value>::splitLess(const SharedPtr<Node> &root, Key key) {
    if (root.get() == nullptr)
        return {root, root};

    if (root->key < key) {
        auto [rightLeft, rightRight] = splitLess(root->right, key);

        SharedPtr node(new Node());
        node->key = root->key;
        node->data = root->data;
        node->left = root->left;
        node->right = rightLeft;
        node->updateSize();

        return {node, rightRight};
    } else {
        auto [leftLeft, leftRight] = splitLess(root->left, key);

        SharedPtr node(new Node());
        node->key = root->key;
        node->data = root->data;
        node->left = leftRight;
        node->right = root->right;
        node->updateSize();

        return {leftLeft, node};
    }
}

template<typename Key, typename Value>
std::pair<SharedPtr<typename LFMap<Key, Value>::Node>, SharedPtr<typename LFMap<Key, Value>::Node>>
LFMap<Key, Value>::splitLessEq(const SharedPtr<Node> &root, Key key) {
    if (root.get() == nullptr)
        return {root, root};

    if (!(key < root->key)) {
        auto [rightLeft, rightRight] = splitLessEq(root->right, key);

        SharedPtr node(new Node());
        node->key = root->key;
        node->data = root->data;
        node->left = root->left;
        node->right = rightLeft;
        node->updateSize();

        return {node, rightRight};
    } else {
        auto [leftLeft, leftRight] = splitLessEq(root->left, key);

        SharedPtr node(new Node());
        node->key = root->key;
        node->data = root->data;
        node->left = leftRight;
        node->right = root->right;
        node->updateSize();

        return {leftLeft, node};
    }
}


} // namespace LFStructs
