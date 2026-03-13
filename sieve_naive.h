#pragma once
#include "cache.h"
#include "types.h"
#include <boost/unordered/concurrent_flat_map.hpp>
#include <cassert>
#include <list>
#include <mutex>
using namespace boost::unordered;
class SIEVENaive : public ICache {
private:
  struct MapData {
    cache_token_t t;
    // set this in order to modify in const function
    // we don't care if multiple visits hit this
    // since writing to bool should be atomic
    mutable bool visited;
  };
  size_t cap;
  std::list<cache_key_t> key_order;
  concurrent_flat_map<cache_key_t, MapData> map;
  std::list<cache_key_t>::iterator hand = key_order.end();
  std::mutex mut;

  void advance_hand() {
    if (key_order.empty())
      return;
    if (hand == key_order.begin()) {
      hand = std::prev(key_order.end());
    } else
      hand--;
  }

  void evict() {
    if (hand == key_order.end())
      hand--;
    while (true) {
      auto k = *hand;
      bool was_vis = false;
      map.cvisit(k, [&](auto &x) {
        if (x.second.visited) {
          was_vis = true;
          x.second.visited = false;
        }
      });
      if (!was_vis) {
        auto to_remove = hand;
        advance_hand();
        map.erase(k);
        key_order.erase(to_remove);
        return;
      }
      advance_hand();
    }
  }

public:
  explicit SIEVENaive(size_t cap) : cap(cap) { assert(cap > 0); }

  cache_token_t query(cache_key_t k, cache_token_t (*get_token)(void *),
                      void *ctx) override {
    cache_token_t t;
    bool hit = map.cvisit(k, [&](auto &x) {
      t = x.second.t;
      x.second.visited = true;
    });
    if (hit)
      return t;
    t = get_token(ctx);
    std::lock_guard lock{mut};

    if (key_order.size() == cap)
      evict();
    key_order.push_front(k);
    bool did = map.try_emplace(k, MapData{t, false});
    if (!did) {
      key_order.pop_front();
      return t;
    }
    return t;
  }

  size_t get_cap() const override { return cap; }
  static bool can_multithread() { return true; }
};