#pragma once
#include "cache.h"
#include "types.h"
#include <atomic>
#include <bit>
#include <bitset>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <cassert>
#include <list>
#include <mutex>
#include <queue>
using namespace boost::unordered;

const int W = 64;
using T = uint64_t;

template <int K> class SIEVEBit : public ICache {
private:
  struct Block {
    T vis[K], in[K];
    int num_in;
    std::array<cache_key_t, W * K> ks;
    void set_vis(int i) { vis[i / W] |= T(1) << (i % W); }
    void set_in(int i) {
      in[i / W] |= T(1) << (i % W);
      num_in++;
    }
    void clr_in(int i) {
      in[i / W] &= ~(T(1) << (i % W));
      num_in--;
    }
    void reset() {
      memset(vis, 0, sizeof vis);
      memset(in, 0, sizeof in);
      num_in = 0;
    }
    bool empty() { return num_in == 0; }
    int nxt_and_clr(int st) {
      for (int b = st / W; b < K; b++) {
        if (b == st / W) {
          T any = (~vis[b] & in[b]) >> (st % W);
          if (any) {
            int out = std::countr_zero(any) + st;
            T skip = (any & -any) - 1;
            vis[b] &= ~(skip << (st % W));
            return out;
          } else {
            vis[b] &= (T(1) << (st % W)) - 1;
          }
        } else {
          T any = ~vis[b] & in[b];
          if (any) {
            // clear all before
            vis[b] &= ~((any & -any) - 1);
            return std::countr_zero(any) + b * W;
          } else {
            // this whole block is visited
            vis[b] = 0;
          }
        }
      }
      return W * K;
    }
  };
  struct ListData {
    Block b;
    int nxt, prv;
  };
  struct MapData {
    Block *b;
    cache_token_t t;
    size_t i;
  };
  int head{}, hand_l{};
  int hand_i{};
  size_t head_num = 0;
  int64_t cap;
  int64_t siz{};
  unordered_flat_map<cache_key_t, MapData> map;

  std::vector<ListData> buf;
  std::vector<int> free;

public:
  explicit SIEVEBit(size_t cap) : cap(cap), buf(cap) {
    assert(cap > 0);
    assert(cap < 1e9);
    hand_l = head;
    hand_i = 0;
    for (size_t i = 1; i < cap; i++)
      free.push_back(i);
  }

  cache_token_t query(cache_key_t k, cache_token_t (*get_token)(void *),
                      void *ctx) override {
    cache_token_t t;
    auto itr = map.find(k);
    if (itr != map.end()) {
      MapData &dat = itr->second;
      dat.b->set_vis(dat.i);
      return dat.t;
    }
    while (siz >= cap) {
      // evict smth
      if (hand_i == W * K) {
        hand_l = buf[hand_l].prv;
        hand_i = 0;
      }
      hand_i = buf[hand_l].b.nxt_and_clr(hand_i);
      if (hand_i != W * K) {
        buf[hand_l].b.clr_in(hand_i);
        map.erase(buf[hand_l].b.ks[hand_i]);
        siz--;
        hand_i++;
        if (hand_l != head && buf[hand_l].b.empty()) {
          auto prv = buf[hand_l].prv, nxt = buf[hand_l].nxt;
          buf[prv].nxt = nxt;
          buf[nxt].prv = prv;
          free.push_back(hand_l);
          hand_l = prv;
          hand_i = 0;
        }
      }
    }
    t = get_token(ctx);
    if (head_num == W * K) {
      // make new head
      int nhead;
      assert(free.size());
      nhead = free.back();
      free.pop_back();
      buf[nhead].b.reset();
      auto prv = buf[head].prv, nxt = head;
      buf[prv].nxt = nhead;
      buf[nhead].prv = prv;
      buf[nxt].prv = nhead;
      buf[nhead].nxt = nxt;
      head = nhead;
      head_num = 0;
    }
    // put into head
    buf[head].b.set_in(head_num);
    buf[head].b.ks[head_num] = k;
    map[k] = {&buf[head].b, t, head_num};
    head_num++;
    siz++;
    assert(map.size() <= cap);

    return t;
  }

  size_t get_cap() const override { return cap; }
  static bool can_multithread() { return false; }
};