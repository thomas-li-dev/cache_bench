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
    ListData *nxt, *prv;
  };
  struct MapData {
    Block *b;
    cache_token_t t;
    size_t i;
  };
  ListData *head, *hand_l;
  size_t hand_i{};
  size_t head_num = 0;
  int64_t cap;
  int64_t siz{};
  unordered_flat_map<cache_key_t, MapData> map;

  std::queue<ListData *> free;

public:
  explicit SIEVEBit(size_t cap) : cap(cap) {
    assert(cap > 0);
    head = new ListData{{}, nullptr, nullptr};
    head->nxt = head->prv = head;
    hand_l = head;
    hand_i = 0;
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
    if (siz == cap) {
      // evict smth
      if (hand_i == W * K) {
        hand_l = hand_l->prv;
        hand_i = 0;
      }
      hand_i = hand_l->b.nxt_and_clr(hand_i);
      if (hand_i != W * K) {
        hand_l->b.clr_in(hand_i);
        map.erase(hand_l->b.ks[hand_i]);
        siz--;
        hand_i++;
        if (hand_l != head && hand_l->b.empty()) {
          auto prv = hand_l->prv, nxt = hand_l->nxt;
          prv->nxt = nxt;
          nxt->prv = prv;
          free.push(hand_l);
          hand_l = prv;
          hand_i = 0;
        }
      }
    }
    t = get_token(ctx);
    if (head_num == W * K) {
      // make new head
      ListData *nhead;
      if (free.size()) {
        nhead = free.front();
        free.pop();
      } else
        nhead = new ListData;
      auto prv = head->prv, nxt = head;
      prv->nxt = nhead;
      nhead->prv = prv;
      nxt->prv = nhead;
      nhead->nxt = nxt;

      head = nhead;
      head_num = 0;
    }
    // put into head
    head->b.set_in(head_num);
    head->b.ks[head_num] = k;
    map[k] = {&head->b, t, head_num};
    head_num++;
    siz++;

    return t;
  }

  size_t get_cap() const override { return cap; }
  static bool can_multithread() { return false; }
};