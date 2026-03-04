#pragma once
#include <cstdint>

using key = uint64_t;
using token = uint64_t;

static inline token get_token_from_secret(key k, uint64_t secret) {
  // source: https://oi-wiki.org/graph/tree-hash/
  k ^= secret;
  k ^= secret;
  k ^= k << 13;
  k ^= k >> 7;
  k ^= k << 17;
  k ^= secret;
  return k;
}
