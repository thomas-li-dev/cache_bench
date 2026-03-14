#pragma once
#include "types.h"
#include <boost/unordered/unordered_flat_map.hpp>
#include <cassert>
#include <vector>
using namespace boost::unordered;
class SIEVESingle {
private:
  std::vector<cache_key_t> keys;
  std::vector<cache_token_t> tokens;
  std::vector<int> nxt;
  std::vector<int> prv;
  std::vector<uint64_t> vis;
  std::vector<int> free_list;
  int head, hand;
  int64_t cap;
  int64_t siz{};
  unordered_flat_map<cache_key_t, int> map;

  void set_vis(int i) { vis[i >> 6] |= uint64_t(1) << (i & 63); }
  void clr_vis(int i) { vis[i >> 6] &= ~(uint64_t(1) << (i & 63)); }
  bool get_vis(int i) { return (vis[i >> 6] >> (i & 63)) & 1; }

  int alloc_node() {
    if (!free_list.empty()) {
      int idx = free_list.back();
      free_list.pop_back();
      return idx;
    }
    int idx = (int)keys.size();
    keys.push_back(0);
    tokens.push_back(0);
    nxt.push_back(0);
    prv.push_back(0);
    if ((idx & 63) == 0)
      vis.push_back(0);
    return idx;
  }

  void free_node(int idx) {
    clr_vis(idx);
    free_list.push_back(idx);
  }

public:
  explicit SIEVESingle(size_t cap) : cap(cap) {
    assert(cap > 0);
    keys.reserve(cap + 2);
    tokens.reserve(cap + 2);
    nxt.reserve(cap + 2);
    prv.reserve(cap + 2);
    vis.reserve((cap + 66) / 64);
    free_list.reserve(cap + 2);
    // sentinel at index 0
    keys.push_back(0);
    tokens.push_back(0);
    nxt.push_back(0);
    prv.push_back(0);
    vis.push_back(0);
    head = 0;
    nxt[head] = head;
    prv[head] = head;
    hand = head;
  }

  cache_token_t query(cache_key_t k, cache_token_t (*get_token)(void *),
                      void *ctx) {
    auto itr = map.find(k);
    if (itr != map.end()) {
      int idx = itr->second;
      set_vis(idx);
      return tokens[idx];
    }
    if (siz == cap) {
      while (true) {
        if (hand == head)
          hand = prv[head];
        if (get_vis(hand)) {
          clr_vis(hand);
          hand = prv[hand];
        } else
          break;
      }

      cache_key_t to_evict = keys[hand];
      int p = prv[hand], n = nxt[hand];
      nxt[p] = n;
      prv[n] = p;
      siz--;
      map.erase(to_evict);
      int old_hand = hand;
      hand = p;
      free_node(old_hand);
    }
    cache_token_t t = get_token(ctx);
    int idx = alloc_node();
    keys[idx] = k;
    tokens[idx] = t;
    map[k] = idx;
    siz++;
    int n = nxt[head];
    nxt[head] = idx;
    prv[n] = idx;
    nxt[idx] = n;
    prv[idx] = head;

    return t;
  }

  size_t get_cap() const { return cap; }
  static constexpr bool can_multithread() { return false; }
};
