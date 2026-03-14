#pragma once
#include "cache.h"
#include "types.h"
#include <bit>
#include <boost/unordered/unordered_flat_map.hpp>
#include <cassert>
#include <cstring>
#include <vector>
using namespace boost::unordered;

const int W = 64;
using T = uint64_t;

template <int K> class SIEVEBit : public ICache {
private:
  static constexpr int BLOCK_SZ = W * K;
  static constexpr int COMPACT = BLOCK_SZ / 2;
  struct Block {
    T vis[K], in[K];
    int num_in;
    std::array<cache_key_t, BLOCK_SZ> ks;
    void set_vis(int i) { vis[i >> 6] |= T(1) << (i & 63); }
    void set_in(int i) {
      in[i >> 6] |= T(1) << (i & 63);
      num_in++;
    }
    void clr_in(int i) {
      in[i >> 6] &= ~(T(1) << (i & 63));
      num_in--;
    }
    void reset() {
      memset(vis, 0, sizeof vis);
      memset(in, 0, sizeof in);
      num_in = 0;
    }
    int nxt_and_clr(int st) {
      for (int b = st >> 6; b < K; b++) {
        if (b == st >> 6) {
          int shift = st & 63;
          T any = (~vis[b] & in[b]) >> shift;
          if (any) {
            int out = std::countr_zero(any) + st;
            T skip = (any & -any) - 1;
            vis[b] &= ~(skip << shift);
            return out;
          } else {
            vis[b] &= (T(1) << shift) - 1;
          }
        } else {
          T any = ~vis[b] & in[b];
          if (any) {
            vis[b] &= ~((any & -any) - 1);
            return std::countr_zero(any) + b * W;
          } else {
            vis[b] = 0;
          }
        }
      }
      return BLOCK_SZ;
    }
  };
  struct ListNode {
    Block b;
    int nxt, prv;
  };
  struct MapData {
    cache_token_t t;
    int block;
    int i;
  };
  std::vector<ListNode> nodes;
  std::vector<int> free_list;
  int head;
  int hand_l;
  int hand_i{};
  int head_num = 0;
  int64_t cap;
  int64_t siz{};
  unordered_flat_map<cache_key_t, MapData> map;

  int alloc_block() {
    if (!free_list.empty()) {
      int idx = free_list.back();
      free_list.pop_back();
      return idx;
    }
    int idx = (int)nodes.size();
    nodes.push_back({});
    return idx;
  }

  void free_block(int idx) { free_list.push_back(idx); }

  std::pair<int, int> ins(cache_key_t k) {
    if (head_num == BLOCK_SZ) {
      int nhead = alloc_block();
      nodes[nhead].b.reset();
      int prv = nodes[head].prv;
      nodes[prv].nxt = nhead;
      nodes[nhead].prv = prv;
      nodes[head].prv = nhead;
      nodes[nhead].nxt = head;

      head = nhead;
      head_num = 0;
    }
    nodes[head].b.set_in(head_num);
    nodes[head].b.ks[head_num] = k;
    return {head, head_num++};
  }

public:
  explicit SIEVEBit(size_t cap) : cap(cap) {
    assert(cap > 0);
    // since each block has >= COMPACT elements except head
    // cap / COMPACT + c
    nodes.reserve(cap / COMPACT + 3);
    nodes.push_back({});
    nodes[0].b.reset();
    nodes[0].nxt = 0;
    nodes[0].prv = 0;
    head = 0;
    hand_l = 0;
    hand_i = 0;
  }

  cache_token_t query(cache_key_t k, cache_token_t (*get_token)(void *),
                      void *ctx) override {
    auto itr = map.find(k);
    if (itr != map.end()) {
      MapData &dat = itr->second;
      nodes[dat.block].b.set_vis(dat.i);
      return dat.t;
    }
    while (siz >= cap) {
      if (hand_i == BLOCK_SZ) {
        hand_l = nodes[hand_l].prv;
        hand_i = 0;
      }
      hand_i = nodes[hand_l].b.nxt_and_clr(hand_i);
      if (hand_i != BLOCK_SZ) {
        nodes[hand_l].b.clr_in(hand_i);
        map.erase(nodes[hand_l].b.ks[hand_i]);
        siz--;
        hand_i++;
        if (hand_l != head && nodes[hand_l].b.num_in < COMPACT) {
          for (int b = 0; b < K; b++) {
            T rem = nodes[hand_l].b.in[b];
            while (rem) {
              int pos = b * W + std::countr_zero(rem);
              auto pos_k = nodes[hand_l].b.ks[pos];
              auto [new_blk, new_i] = ins(pos_k);

              auto itr = map.find(pos_k);
              itr->second.block = new_blk;
              itr->second.i = new_i;

              rem &= rem - 1;
            }
          }
          int prv = nodes[hand_l].prv, nxt = nodes[hand_l].nxt;
          nodes[prv].nxt = nxt;
          nodes[nxt].prv = prv;
          free_block(hand_l);
          hand_l = prv;
          hand_i = 0;
        }
      }
    }
    cache_token_t t = get_token(ctx);
    auto [blk, slot] = ins(k);
    map[k] = {t, blk, slot};
    siz++;

    return t;
  }

  size_t get_cap() const override { return cap; }
  static bool can_multithread() { return false; }
};
