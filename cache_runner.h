#pragma once
#include "types.h"
#include <barrier>
#include <chrono>
#include <cstdio>
#include <pthread.h>
#include <span>
#include <thread>
#include <x86intrin.h>

struct QueryStats {
  std::chrono::duration<double, std::nano> runtime{};
  size_t queries{}, hits{};
  // cycles
  std::vector<uint64_t> samples;
};

enum class scale_policy { INTERLEAVE = 0, TRANSFORM_SPACE = 1, REPLICATE = 2 };

constexpr std::string_view scale_policy_name(scale_policy sp) {
  switch (sp) {
  case scale_policy::INTERLEAVE:
    return "interleave";
  case scale_policy::TRANSFORM_SPACE:
    return "transform_space";
  case scale_policy::REPLICATE:
    return "replicate";
  }
  return "unknown";
}

template <class Cache> class CacheRunner {
private:
  std::string name;
  Cache cache;
  uint64_t secret;
  size_t num_threads;
  scale_policy sp;
  std::vector<int> cpu_order;

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
    struct QueryCtx {
      bool *missed;
      cache_token_t expected;
    } qctx{&missed, expected};
    cache_token_t t = cache.query(
        k,
        [](void *ctx) -> cache_token_t {
          auto *q = static_cast<QueryCtx *>(ctx);
          *q->missed = true;
          return q->expected;
        },
        &qctx);
    if (sample) {
      _mm_lfence();
      end_cyc = __rdtsc();
      stats.samples.push_back(end_cyc - start_cyc);
    }

    [[unlikely]]
    if (t != expected) {
      // TODO: we don't have much context for this.
      printf("cache %s returned wrong value???\n", name.c_str());
      std::exit(1);
    }

    // we don't know what cache does. If it
    // queries the token multiple times for some reason
    // we don't want to count multiple misses (debatable)
    if (!missed)
      stats.hits++;
  }

public:
  CacheRunner(std::string name, size_t cap, uint64_t secret,
              size_t num_threads, scale_policy sp,
              std::span<const int> cpu_order)
      : name(std::move(name)), cache(cap), secret(secret),
        num_threads(num_threads), sp(sp),
        cpu_order(cpu_order.begin(), cpu_order.end()) {}

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
              CPU_SET(cpu_order[tid], &cpuset);
              pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t),
                                     &cpuset);
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
              } else if (sp == scale_policy::REPLICATE) {
                for (size_t j = 0; j < buf.size(); j++) {
                  do_query(buf[j], stats[tid]);
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
      for (size_t j = 0; j < buf.size(); j++) {
        do_query(buf[j], stats);
      }
      auto end = std::chrono::high_resolution_clock::now();
      stats.runtime = std::chrono::duration<double, std::nano>(end - start);
      return stats;
    }
  }

  size_t get_cap() const { return cache.get_cap(); }
};
