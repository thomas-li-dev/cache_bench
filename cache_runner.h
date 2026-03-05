#pragma once
#include "cache.h"
#include "fun_timer.h"
#include <memory>
#include <queue>
#include <span>

// TODO: do we need bench -> cacherunner -> cache?
// maybe combining first two is better?
struct QueryStats {
  std::chrono::duration<double, std::nano> runtime;
  size_t queries, hits;
};
class CacheRunner {
private:
  std::string name;
  std::unique_ptr<ICache> cache;
  uint64_t secret;

  bool do_query(cache_key_t k) {
    // TODO: add sampling for latency.
    bool missed = false;
    cache_token_t t = cache->query(k, [&](cache_key_t k) {
      missed = true;
      return get_token_from_secret(k, secret);
    });
    cache_token_t expected = get_token_from_secret(k, secret);

    [[unlikely]]
    if (t != expected) {
      // TODO: we don't have much context for this.
      std::println("cache {} returned wrong value???", name);
      std::exit(1);
    }

    // we don't know what cache does. If it
    // queries the token multiple times for some reason
    // we don't want to count multiple misses (debatable)
    return !missed;
  }

public:
  CacheRunner(std::string name, std::unique_ptr<ICache> cache, uint64_t secret)
      : name(name), cache(std::move(cache)), secret(secret) {}
  // TODO: runtime polymorphism has overhead.
  // Although this might be better actually.
  // Avoid compiler inlining function or anything specific
  // to the testing framework. (like using the fact we run this repeatedly)

  QueryStats do_queries(std::span<cache_key_t> buf) {
    QueryStats stats{};
    auto start = std::chrono::high_resolution_clock::now();
    for (auto &k : buf) {
      stats.queries++;
      bool hit = do_query(k);
      stats.hits += hit;
    }
    auto end = std::chrono::high_resolution_clock::now();
    stats.runtime = std::chrono::duration<double, std::milli>(end - start);
    return stats;
  }
  std::string_view get_name() const { return name; }
};
