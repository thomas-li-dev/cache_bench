#pragma once
#include "cache.h"
#include "types.h"
#include <atomic>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <cassert>
#include <functional>
#include <list>
#include <mutex>
#include <queue>

using namespace boost::unordered;
class FIFO : public ICache {
private:
  size_t cap;
  std::list<cache_key_t> ord;
  concurrent_flat_map<cache_key_t, cache_token_t> map;
  std::mutex mut;
  std::atomic<int64_t> approx_siz;

public:
  FIFO(size_t cap) : cap(cap) {}

  cache_token_t
  query(cache_key_t k,
        std::function<cache_token_t(cache_key_t)> get_token) override {
    cache_token_t t;
    bool hit = map.cvisit(k, [&](auto &x) { t = x.second; });
    if (hit)
      return t;
    t = get_token(k);
    mut.lock();
    auto itr = ord.insert(ord.end(), k);
    mut.unlock();
    bool inserted = map.try_emplace(k, t);
    if (!inserted) {
      // another thread inserted k
      // before us
      mut.lock();
      ord.erase(itr);
      mut.unlock();
      return t;
    }
    // this is only a rough approximation,
    // so relaxed ordering is fine.
    size_t cur = approx_siz.fetch_add(1, std::memory_order::relaxed) + 1;
    if (cur > cap) {
      mut.lock();
      while (approx_siz.load(std::memory_order::relaxed) > cap) {
        auto k = ord.front();
        ord.pop_front();
        size_t did = map.erase(k);
        if (did)
          approx_siz.fetch_sub(1, std::memory_order::relaxed);
      }
      mut.unlock();
    }
    return t;
  }
  virtual size_t get_cap() const override { return cap; }
  static bool can_multithread() { return true; }
  void reset() override {
    map.clear();
    ord.clear();
  }
};
