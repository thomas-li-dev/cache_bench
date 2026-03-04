#pragma once
#include "cache.h"
#include "fun_timer.h"
#include <memory>

class CacheRunner {
private:
  std::string name;
  std::unique_ptr<ICache> cache;
  Metrics metrics;
  int queries = 0, hits = 0;
  uint64_t secret;

public:
  CacheRunner(std::string name, std::unique_ptr<ICache> cache, uint64_t secret)
      : name(name), cache(std::move(cache)), secret(secret) {}
  // TODO: runtime polymorphism has overhead.
  // Although this might be better actually.
  // Avoid compiler inlining function or anything specific
  // to the testing framework. (like using the fact we run this repeatedly)
  void do_query(key k) {
    queries++;
    bool missed = false;
    auto start = std::chrono::high_resolution_clock::now();
    token t = cache->query(k, [&](key k) {
      missed = true;
      return get_token_from_secret(k, secret);
    });
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::micro>(end - start);
    metrics.add(duration.count());
    token expected = get_token_from_secret(k, secret);
    if (t != expected) {
      // TODO: we don't have much context for this.
      std::println("cache {} returned wrong value???", name);
      std::exit(1);
    }

    // we don't know what cache does. If it
    // queries the token multiple times for some reason
    // we don't want to count multiple misses (debatable)
    if (!missed)
      hits++;
  }
  void print() {
    std::println("======== {} =======", name);
    if (queries == 0) {
      std::println("no queries");
      return;
    }
    metrics.print();
    std::println("hitrate = {}", 1.0 * hits / queries);
  }
  void reset() { metrics.reset(); }
};
