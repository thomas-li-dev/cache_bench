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

class Bench {
private:
  std::vector<CacheRunner> caches;
  std::vector<Trace> traces;
  uint64_t secret = std::random_device()();

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

    for (auto &trace : traces) {
      for (auto &cache : caches) {
        cache.reset();
      }
      std::println("running trace {}", trace.get_name());
      std::vector<cache_key_t> buf;
      for (size_t batch = 0;; batch++) {
        auto start = std::chrono::high_resolution_clock::now();
        trace.next_block(buf);
        auto end = std::chrono::high_resolution_clock::now();
        if (buf.empty())
          break;
        std::println("batch {} start", batch);
        std::println(
            "loaded queries into buf in {} ms",
            std::chrono::duration<double, std::milli>(end - start).count());
        for (auto &cache : caches) {
          QueryStats stats = cache.do_queries(buf);
          nlohmann::json results;
          results["hit_rate"] = 1.0 * stats.hits / stats.queries;
          results["avg_latency_ns"] =
              1.0 * stats.runtime.count() / stats.queries;
          results["throughput_qps"] =
              1.0 * stats.queries / stats.runtime.count() * 1e9;
          // std::println("batch results: {}", batch_results.dump());

          // maybe better for trace to be first dim?
          results["cache_name"] = cache.get_name();
          results["trace_name"] = trace.get_name();
          results["threads"] = cache.get_threads();
          results["batch"] = batch;
          results["capacity"] = cache.get_cap();
          out << results.dump() << "\n";
        }
      }
    }
  }

  template <class T, class... Args>
  void add_cache(const std::string &name, Args &&...args) {
    for (size_t threads : threads_choices) {
      if (threads > 1 && !T::can_multithread())
        continue;
      for (size_t cap : cap_choices) {
        caches.push_back(
            CacheRunner(name, std::make_unique<T>(cap), secret, threads));
      }
    }
  }
  void add_trace(const std::string &name, const fs::path &path) {
    traces.push_back({name, path});
  }
};