#pragma once
#include "types.h"
#include <atomic>
#include <boost/lockfree/queue.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>
using namespace boost::unordered;

constexpr int k = 2;
class FIFOBatch {
private:
  int64_t cap;
  boost::lockfree::queue<cache_key_t> ord;
  concurrent_flat_map<cache_key_t, cache_token_t> map;
  std::atomic<int64_t> siz{};

public:
  FIFOBatch(size_t cap) : cap(cap), ord(cap * 2) {}
  cache_token_t query(cache_key_t k, cache_token_t (*get_token)(void *),
                      void *ctx) {
    cache_token_t t;
    bool hit = map.cvisit(k, [&](auto &x) { t = x.second; });
    if (hit)
      return t;
    t = get_token(ctx);
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
    if (siz.load(std::memory_order::relaxed) > k * cap) {
      while (siz.load(std::memory_order::relaxed) > cap) {
        bool popped = ord.pop(evict_key);
        if (!popped)
          break;
        siz.fetch_sub(1, std::memory_order::relaxed);
        map.erase(evict_key);
      }
    }

    return t;
  }
  size_t get_cap() const { return cap; }
  static constexpr bool can_multithread() { return true; }
};
