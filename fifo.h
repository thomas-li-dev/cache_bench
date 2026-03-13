#pragma once
#include "cache.h"
#include "types.h"
#include <atomic>
#include <boost/lockfree/queue.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <functional>

using namespace boost::unordered;
class FIFO : public ICache {
private:
  int64_t cap;
  boost::lockfree::queue<cache_key_t> ord;
  concurrent_flat_map<cache_key_t, cache_token_t> map;
  std::atomic<int64_t> siz{};

public:
  FIFO(size_t cap) : cap(cap), ord(cap * 2) {}

  cache_token_t query(cache_key_t k,
                      std::function<cache_token_t()> get_token) override {
    cache_token_t t;
    bool hit = map.cvisit(k, [&](auto &x) { t = x.second; });
    if (hit)
      return t;
    t = get_token();
    bool inserted = map.emplace(k, t);
    if (!inserted)
      return t;
    bool pushed = ord.bounded_push(k);
    if (!pushed) {
      map.erase(k);
      return t;
    }
    siz.fetch_add(1, std::memory_order::relaxed);

    cache_key_t evict_key;
    while (siz.load(std::memory_order::relaxed) > cap) {
      bool popped = ord.pop(evict_key);
      if (!popped)
        break;
      siz.fetch_sub(1, std::memory_order::relaxed);
      map.erase(evict_key);
    }

    return t;
  }
  virtual size_t get_cap() const override { return cap; }
  static bool can_multithread() { return true; }
};
