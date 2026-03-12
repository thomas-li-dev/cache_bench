#pragma once
#include <cstdint>

// WTFFFF key_t is a POSIX type :ICANT:
using cache_key_t = uint64_t;
using cache_token_t = uint64_t;

static inline cache_token_t get_token_from_secret(cache_key_t k,
                                                  uint64_t secret) {
  k += secret;
  k += 0x9e3779b97f4a7c15;
  k = (k ^ (k >> 30)) * 0xbf58476d1ce4e5b9;
  k = (k ^ (k >> 27)) * 0x94d049bb133111eb;
  return k ^ (k >> 31);
}
