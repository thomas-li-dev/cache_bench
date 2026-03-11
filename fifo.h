#pragma once
#include "cache.h"
#include "types.h"
#include <boost/unordered/concurrent_flat_map.hpp>
#include <cassert>
#include <functional>
#include <list>
#include <mutex>

using namespace boost::unordered;
class FIFO : public ICache {
private:
  size_t cap;
  std::list<cache_key_t> ord;
  concurrent_flat_map<cache_key_t, cache_token_t> map;
  std::mutex mut;
  size_t siz{};

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
    std::lock_guard lock{mut};
    auto itr = ord.insert(ord.end(), k);
    bool inserted = map.try_emplace(k, t);
    if (!inserted) {
      ord.erase(itr);
      return t;
    }
    siz++;
    while (siz > cap) {
      auto victim = ord.front();
      ord.pop_front();
      if (map.erase(victim))
        siz--;
    }
    return t;
  }
  virtual size_t get_cap() const override { return cap; }
  static bool can_multithread() { return true; }
};
