// give set of eviction algos, and set of traces
// for each trace, run all eviction algos
// collect metrics
#include "cache_runner.h"
#include "ext/json.hpp"
#include "fifo.h"
#include "trace.h"
#include <chrono>
#include <fstream>
#include <memory>
#include <print>
#include <random>
#include <vector>

// TODO: add multithreading
// one method: generate a trace for each thread
// all threads share a cache.
// question: how to measure? should
// we care about internal part measurements?
// I think not, since not every algo would follow this process
// and with concurrency we might have to make the steps more coupled

// TODO: another issue: our measurements
// assume reasonable cache impl
// if a cache impl doesn't cache anything, then
// it'll be the fastest.
// in practice the performance is related to having
// a good miss ratio.

// idea: assign a "cost" to each miss. Make the cache
// return a token associated with the key.
// then the cost of a cache depends on the miss rate and its own performance
// then we can also verify the correctness of the cache.
// we can then find for each cache, whether it would be on the convex hull
// of the combined metric (for some miss cost)

// can enforce memory as well with allocator?

// we need to work with semi-adversarial cache impls, as there could be bugs.

// other issue is the performance of cache could be determined mostly by
// hash table perf, which isn't interesting.
// Kind of the reason we wanted to measure parts separately.
// maybe look separately at hit / miss performance.

class Bench {
private:
  std::vector<CacheRunner> caches;
  std::vector<std::pair<std::string, std::unique_ptr<ITrace>>> traces;
  uint64_t secret = std::random_device()();

  // TODO: convex hull!!!
  const double miss_cost = 1e6; // 1e6ns = 1 ms
  std::vector<size_t> threads_choices, cap_choices;

public:
  Bench(const std::vector<size_t> &threads_choices,
        const std::vector<size_t> &cap_choices)
      : threads_choices(threads_choices), cap_choices(cap_choices) {}
  void run() {
    // for each cache & num threads & capacity (cache options)
    // for each trace
    // for each batch
    // put stats
    std::ofstream out("results.json");

    for (auto &[trace_name, trace] : traces) {
      std::println("running trace {}", trace_name);
      // TODO: make configurable.
      // probably queries should be configured by the
      // trace parameters (and having to_buf return empty once hitting
      // configured num)

      // choose batch size to be something that fits in memory, but reasonably
      // big as we allocate resources for each batch like jsons, threads, etc.
      const size_t batch_size = 1e5, total_queries = 1e6;
      // we only use vector as a RAII container for this buffer
      // shouldn't reallocate ever.
      std::vector<cache_key_t> buf(batch_size);
      for (size_t num_done = 0, siz; num_done < total_queries;) {
        std::println("batch {} start", num_done / batch_size);
        auto start = std::chrono::high_resolution_clock::now();
        siz = trace->next_buf(buf);
        auto end = std::chrono::high_resolution_clock::now();
        std::println(
            "loaded queries into buf in {} ms",
            std::chrono::duration<double, std::milli>(end - start).count());
        size_t rem = total_queries - num_done;
        if (!siz)
          break;
        siz = std::min(siz, rem);
        num_done += siz;
        std::span<cache_key_t> span(buf.data(), siz);
        for (auto &cache : caches) {
          QueryStats stats = cache.do_queries(span);
          double cost =
              1.0 * stats.runtime.count() / stats.queries +
              1.0 * (stats.queries - stats.hits) / stats.queries * miss_cost;
          nlohmann::json results;
          results["hit_rate"] = 1.0 * stats.hits / stats.queries;
          results["avg_latency_ns"] =
              1.0 * stats.runtime.count() / stats.queries;
          results["throughput_qps"] =
              1.0 * stats.queries / stats.runtime.count() * 1e9;
          results["cost_ns"] = cost;
          // std::println("batch results: {}", batch_results.dump());

          // maybe better for trace to be first dim?
          results["cache_name"] = cache.get_name();
          results["trace_name"] = trace_name;
          results["threads"] = cache.get_threads();
          results["batch"] = num_done / batch_size;
          results["capacity"] = cache.get_cap();
          out << results.dump() << "\n";
        }
      }
    }
  }

  template <class T, class... Args>
  void add_cache(const std::string &name, Args &&...args) {
    for (size_t threads : threads_choices) {
      for (size_t cap : cap_choices) {
        caches.push_back(
            CacheRunner(name, std::make_unique<T>(cap), secret, threads));
      }
    }
  }
  template <class T, class... Args>
  void add_trace(const std::string &name, Args &&...args) {
    traces.push_back({name, std::make_unique<T>(args...)});
  }
};