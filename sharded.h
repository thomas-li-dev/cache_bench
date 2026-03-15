#pragma once
#include "types.h"
#include <cassert>
#include <cstdlib>
#include <mutex>
#include <new>

template <class Cache, int NumShards> class Sharded {
  static_assert((NumShards & (NumShards - 1)) == 0,
                "NumShards must be a power of 2");

private:
  struct alignas(64) Shard {
    Cache cache;
    std::mutex mut;
    Shard(size_t cap) : cache(cap) {}
  };
  Shard *shards;

  int shard_for(cache_key_t k) {
    return (k * 11400714819323198485ULL) >> (64 - __builtin_ctz(NumShards));
  }

public:
  explicit Sharded(size_t cap) {
    assert(cap > 0);
    size_t per_shard = (cap + NumShards - 1) / NumShards;
    // Allocate contiguous aligned memory for all shards
    void *mem = std::aligned_alloc(alignof(Shard), sizeof(Shard) * NumShards);
    shards = static_cast<Shard *>(mem);
    for (int i = 0; i < NumShards; i++)
      new (&shards[i]) Shard(per_shard);
  }

  ~Sharded() {
    for (int i = 0; i < NumShards; i++)
      shards[i].~Shard();
    std::free(shards);
  }

  Sharded(const Sharded &) = delete;
  Sharded &operator=(const Sharded &) = delete;

  cache_token_t query(cache_key_t k, cache_token_t (*get_token)(void *),
                      void *ctx) {
    auto &shard = shards[shard_for(k)];
    if constexpr (Cache::can_multithread()) {
      return shard.cache.query(k, get_token, ctx);
    } else {
      std::lock_guard lock(shard.mut);
      return shard.cache.query(k, get_token, ctx);
    }
  }

  size_t get_cap() const {
    size_t total = 0;
    for (int i = 0; i < NumShards; i++)
      total += shards[i].cache.get_cap();
    return total;
  }

  static constexpr bool can_multithread() { return true; }
};
