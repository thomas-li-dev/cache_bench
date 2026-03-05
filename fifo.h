#pragma once
#include "cache.h"
#include <cassert>
#include <functional>
#include <queue>
#include <unordered_map>

class FIFO : public ICache {
private:
  size_t c;
  std::queue<cache_key_t> q;
  std::unordered_map<cache_key_t, cache_token_t> m;

  void evict() {
    assert(q.size());
    auto k = q.front();
    q.pop();
    m.erase(k);
  }
  bool in(cache_key_t key) { return m.contains(key); }
  void add(cache_key_t key, cache_token_t t) {
    assert(q.size() < c);
    m[key] = t;
    q.push(key);
  }
  bool can_add() const { return m.size() < c; }

public:
  FIFO(size_t cap) : c(cap) {}

  cache_token_t
  query(cache_key_t k,
        std::function<cache_token_t(cache_key_t)> get_token) override {
    if (in(k)) {
      return m[k];
    }
    cache_token_t t = get_token(k);
    if (!can_add())
      evict();
    add(k, t);
    return t;
  }
};
