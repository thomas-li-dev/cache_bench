#pragma once
#include "cache.h"
#include "types.h"
#include <atomic>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <cassert>
#include <functional>
#include <list>
#include <mutex>
using namespace boost::unordered;
class SIEVE : public ICache {
private:
  struct ListData {
    std::atomic<bool> vis;
    cache_key_t k;
    cache_token_t t;
    ListData *nxt, *prv;
  };
  ListData *head, *hand;
  int64_t cap;
  int64_t siz{};
  concurrent_flat_map<cache_key_t, ListData *> map;
  std::mutex mut;

public:
  explicit SIEVE(size_t cap) : cap(cap) {
    assert(cap > 0);
    head = new ListData{0, 0, 0, 0, 0};
    head->nxt = head->prv = head;
    hand = head;
  }

  cache_token_t query(cache_key_t k,
                      std::function<cache_token_t()> get_token) override {
    cache_token_t t;
    bool hit = map.cvisit(k, [&](auto &x) {
      t = x.second->t;
      x.second->vis.store(true, std::memory_order::relaxed);
    });
    if (hit)
      return t;
    t = get_token();
    ListData *ld = new ListData{0, k, t, 0, 0};
    bool added = map.emplace(k, ld);
    if (!added) {
      delete ld;
    } else {
      std::lock_guard lock{mut};
      siz++;
      auto nxt = head->nxt;
      head->nxt = ld;
      nxt->prv = ld;
      ld->nxt = nxt;
      ld->prv = head;
      while (siz > cap) {
        // evict smth
        // to end
        if (hand == head)
          hand = head->prv;
        // head -> head => empty
        // can this happen?
        if (hand == head) {
          assert(0);
          break;
        }
        if (hand->vis.load(std::memory_order::relaxed)) {
          hand->vis.store(false, std::memory_order::relaxed);
          hand = hand->prv;
          continue;
        }
        cache_key_t to_evict = hand->k;
        auto prv = hand->prv, nxt = hand->nxt;
        prv->nxt = nxt;
        nxt->prv = prv;
        siz--;
        size_t sbo = map.erase(to_evict);
        assert(sbo == 1);
        delete hand;
        hand = prv;
      }
    }
    return t;
  }

  size_t get_cap() const override { return cap; }
  static bool can_multithread() { return true; }
};