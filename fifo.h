#pragma once
#include "cache.h"
#include <cassert>
#include <functional>
#include <mutex>
#include <queue>
#include <unordered_map>

class FIFO : public ICache {
private:
  size_t cap;
  std::queue<cache_key_t> ord;
  std::unordered_map<cache_key_t, cache_token_t> map;
  std::mutex mut;
  void evict() {
    assert(ord.size());
    auto k = ord.front();
    ord.pop();
    map.erase(k);
  }
  bool in(cache_key_t key) { return map.contains(key); }
  void add(cache_key_t key, cache_token_t t) {
    assert(ord.size() < cap);
    map[key] = t;
    ord.push(key);
  }
  bool can_add() const { return map.size() < cap; }

public:
  FIFO(size_t cap) : cap(cap) {}

  cache_token_t
  query(cache_key_t k,
        std::function<cache_token_t(cache_key_t)> get_token) override {
    std::lock_guard lock(mut);
    if (in(k)) {
      return map[k];
    }
    cache_token_t t = get_token(k);
    if (!can_add())
      evict();
    add(k, t);
    return t;
  }
  virtual size_t get_cap() const override { return cap; }
};
