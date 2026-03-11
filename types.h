#pragma once
#include <cstdint>

// WTFFFF key_t is a POSIX type :ICANT:
using cache_key_t = uint64_t;
using cache_token_t = uint64_t;

static inline cache_token_t get_token_from_secret(cache_key_t k,
                                                  uint64_t secret) {
  // source: https://oi-wiki.org/graph/tree-hash/
  k ^= secret;
  k ^= k << 13;
  k ^= k >> 7;
  k ^= k << 17;
  k ^= secret;
  return k;
}
