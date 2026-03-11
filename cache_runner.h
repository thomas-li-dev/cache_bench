#pragma once
#include "cache.h"
#include <barrier>
#include <chrono>
#include <memory>
#include <print>
#include <span>
#include <thread>

// TODO: do we need bench -> cacherunner -> cache?
// maybe combining first two is better?
struct QueryStats {
  std::chrono::duration<double, std::nano> runtime{};
  size_t queries{}, hits{};
};
class CacheRunner {
private:
  std::string name;
  std::unique_ptr<ICache> cache;
  uint64_t secret;
  size_t num_threads;
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
  CacheRunner(std::string name, std::unique_ptr<ICache> cache, uint64_t secret,
              size_t num_threads)
      : name(name), cache(std::move(cache)), secret(secret),
        num_threads(num_threads) {}
  // TODO: runtime polymorphism has overhead.
  // Although this might be better actually.
  // Avoid compiler inlining function or anything specific
  // to the testing framework. (like using the fact we run this repeatedly)

  QueryStats do_queries(std::span<cache_key_t> buf) {
    if (num_threads > 1) {
      std::vector<std::thread> threads;
      std::vector<QueryStats> stats(num_threads);
      std::chrono::time_point<std::chrono::high_resolution_clock> rt_start,
          rt_end;
      auto on_start = [&]() {
        rt_start = std::chrono::high_resolution_clock::now();
      };
      auto on_end = [&]() {
        rt_end = std::chrono::high_resolution_clock::now();
      };
      std::barrier start_barrier(num_threads, on_start);
      std::barrier end_barrier(num_threads, on_end);
      for (size_t i = 0; i < num_threads; i++) {
        threads.push_back(std::thread(
            [&](size_t tid) {
              start_barrier.arrive_and_wait();
              auto start = std::chrono::high_resolution_clock::now();
              for (size_t j = tid; j < buf.size(); j += num_threads) {
                stats[tid].queries++;
                bool hit = do_query(buf[j]);
                stats[tid].hits += hit;
              }
              auto end = std::chrono::high_resolution_clock::now();
              end_barrier.arrive_and_wait();
              stats[tid].runtime =
                  std::chrono::duration<double, std::nano>(end - start);
            },
            i));
      }
      for (auto &thread : threads) {
        thread.join();
      }
      QueryStats total_stats;

      // runtime is the "real time" like in time cmd
      total_stats.runtime =
          std::chrono::duration<double, std::nano>(rt_end - rt_start);
      for (size_t i = 0; i < num_threads; i++) {
        total_stats.queries += stats[i].queries;
        total_stats.hits += stats[i].hits;
      }
      return total_stats;
    } else {
      // special path for single thread.
      QueryStats stats;
      auto start = std::chrono::high_resolution_clock::now();
      for (size_t j = 0; j < buf.size(); j += num_threads) {
        stats.queries++;
        bool hit = do_query(buf[j]);
        stats.hits += hit;
      }
      auto end = std::chrono::high_resolution_clock::now();
      stats.runtime = std::chrono::duration<double, std::nano>(end - start);
      return stats;
    }
  }
  std::string_view get_name() const { return name; }
  size_t get_threads() const { return num_threads; }
  size_t get_cap() const { return cache->get_cap(); }
  void reset() { cache->reset(); }
};
