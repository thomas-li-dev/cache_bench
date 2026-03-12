#pragma once
#include "cache.h"
#include "types.h"
#include <barrier>
#include <chrono>
#include <memory>
#include <print>
#include <span>
#include <pthread.h>
#include <thread>
#include <x86intrin.h>

// TODO: do we need bench -> cacherunner -> cache?
// maybe combining first two is better?
struct QueryStats {
  std::chrono::duration<double, std::nano> runtime{};
  size_t queries{}, hits{};
  // cycles
  std::vector<uint64_t> samples;
};

enum class scale_policy { INTERLEAVE = 0, TRANSFORM_SPACE = 1 };

class CacheRunner {
private:
  std::string name;
  std::unique_ptr<ICache> cache;
  uint64_t secret;
  size_t num_threads;
  scale_policy sp;
  double cap_prop;

  void do_query(cache_key_t k, QueryStats &stats) {
    cache_token_t expected = get_token_from_secret(k, secret);
    bool missed = false;
    // sample every 1/1024 queries
    bool sample = stats.queries++ % 1024 == 0;
    uint64_t start_cyc, end_cyc;
    if (sample) {
      _mm_lfence();
      start_cyc = __rdtsc();
    }
    cache_token_t t = cache->query(k, [&]() {
      missed = true;
      return expected;
    });
    if (sample) {
      _mm_lfence();
      end_cyc = __rdtsc();
      stats.samples.push_back(end_cyc - start_cyc);
    }

    [[unlikely]]
    if (t != expected) {
      // TODO: we don't have much context for this.
      std::println("cache {} returned wrong value???", name);
      std::exit(1);
    }

    // we don't know what cache does. If it
    // queries the token multiple times for some reason
    // we don't want to count multiple misses (debatable)
    if (!missed)
      stats.hits++;
  }

public:
  CacheRunner(std::string name, std::unique_ptr<ICache> cache, uint64_t secret,
              size_t num_threads, scale_policy sp, double cap_prop)
      : name(name), cache(std::move(cache)), secret(secret),
        num_threads(num_threads), sp(sp), cap_prop(cap_prop) {}
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
              cpu_set_t cpuset;
              CPU_ZERO(&cpuset);
              CPU_SET(tid, &cpuset);
              pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
              start_barrier.arrive_and_wait();
              auto start = std::chrono::high_resolution_clock::now();
              if (sp == scale_policy::INTERLEAVE) {
                for (size_t j = tid; j < buf.size(); j += num_threads) {
                  do_query(buf[j], stats[tid]);
                }
              } else if (sp == scale_policy::TRANSFORM_SPACE) {
                for (size_t j = 0; j < buf.size(); j++) {
                  cache_key_t k = buf[j];
                  k = get_token_from_secret(k, tid);
                  do_query(k, stats[tid]);
                }
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
        total_stats.samples.insert(total_stats.samples.end(),
                                   stats[i].samples.begin(),
                                   stats[i].samples.end());
      }
      return total_stats;
    } else {
      // special path for single thread.
      QueryStats stats;
      auto start = std::chrono::high_resolution_clock::now();
      for (size_t j = 0; j < buf.size(); j += num_threads) {
        do_query(buf[j], stats);
      }
      auto end = std::chrono::high_resolution_clock::now();
      stats.runtime = std::chrono::duration<double, std::nano>(end - start);
      return stats;
    }
  }
  std::string_view get_name() const { return name; }
  size_t get_threads() const { return num_threads; }
  size_t get_cap() const { return cache->get_cap(); }
  double get_cap_prop() const { return cap_prop; }
};
