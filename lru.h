#pragma once
#include "types.h"
#include <cassert>
#include <list>
#include <mutex>
#include <queue>
#include <unordered_map>
class LRU {
private:
  size_t cap;
  std::list<cache_key_t> order;
  std::unordered_map<cache_key_t,
                     std::pair<cache_token_t, std::list<cache_key_t>::iterator>>
      map;
  std::mutex mut;
  void evict() {
    assert(order.size());
    auto k = order.back();
    order.pop_back();
    map.erase(k);
  }
  bool in(cache_key_t key) { return map.contains(key); }
  void add(cache_key_t key, cache_token_t t) {
    assert(order.size() < cap);
    auto itr = order.insert(order.begin(), key);
    map[key] = {t, itr};
  }
  bool can_add() const { return map.size() < cap; }

public:
  LRU(size_t cap) : cap(cap) {}

  cache_token_t query(cache_key_t k, cache_token_t (*get_token)(void *),
                      void *ctx) {
    std::lock_guard lock(mut);
    if (in(k)) {
      auto [t, itr] = map[k];
      order.splice(order.begin(), order, itr);
      return t;
    }
    cache_token_t t = get_token(ctx);
    if (!can_add())
      evict();
    add(k, t);
    return t;
  }
  size_t get_cap() const { return cap; }
  static constexpr bool can_multithread() { return true; }
};