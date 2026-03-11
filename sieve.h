#pragma once
#include "cache.h"
#include <cassert>
#include <functional>
#include <list>
#include <mutex>
#include <unordered_map>

struct Node {
    cache_key_t key;
    cache_token_t token;
    bool visited = false;
};

class SIEVE : public ICache {
private:
    size_t cap;
    std::list<Node> nodeList;
    std::unordered_map<cache_key_t, std::list<Node>::iterator> map;
    std::list<Node>::iterator hand = nodeList.end();
    std::mutex mut;

    bool in(cache_key_t key) const { return map.contains(key); }
    bool can_add() const { return map.size() < cap; }

    void advance_hand() {
        if (nodeList.empty()) {
            hand = nodeList.end();
            return;
        }

        if (hand == nodeList.end()) {
            hand = std::prev(nodeList.end());  // start at back
            return;
        }

        if (hand == nodeList.begin()) {
            hand = std::prev(nodeList.end());  // wrap front -> back
            return;
        }

        --hand;
    }

    void add(cache_key_t key, cache_token_t token) {
        assert(nodeList.size() < cap);

        nodeList.push_front(Node{key, token, false});
        map[key] = nodeList.begin();

        if (nodeList.size() == 1)
        hand = nodeList.begin();
    }

    void evict() {
        while (true) {
            if (hand == nodeList.end())
                hand = nodeList.begin();

            if (!hand->visited) {
                auto to_remove = hand;
                advance_hand();
                map.erase(ro_remove->key);
                nodeList.erase(to_remove);

                if (nodeList.empty())
                hand = nodeList.end();

                return;
            }

            hand->visited = false;
            advance_hand();
        }
    }

public:
    explicit SIEVE(size_t cap) : cap(cap) { assert(cap > 0); }

    cache_token_t
    query(cache_key_t k,
        std::function<cache_token_t(cache_key_t)> get_token) override {
            // lock while we look for element 
            std::lock_guard<std::mutex> lock(mut);

            // can this hot path avoid a lock??
            auto it = map.find(k);
            if (it != map.end()) {
            it->second->visited = true;
            return it->second->token;
            }

            cache_token_t t = get_token(k);

            // evict according to docs
            if (!can_add())
            evict();

            add(k, t);
            return t;
    }

    size_t get_cap() const override { return cap; }

    static bool can_multithread() { return true; }
};