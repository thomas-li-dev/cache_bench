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
  struct CacheMaker {
    std::string name;
    std::function<std::unique_ptr<ICache>(size_t)> maker;
  };
  std::vector<CacheMaker> cache_makers;
  std::vector<Trace> traces;
  uint64_t secret = std::random_device()();

  std::vector<size_t> threads_choices;
  std::vector<double> cap_prop;

public:
  Bench(const std::vector<size_t> &threads_choices,
        const std::vector<double> &cap_prop)
      : threads_choices(threads_choices), cap_prop(cap_prop) {}
  void run() {
    // calibrate TSC frequency first.
    // could do with syscall?
    auto start = std::chrono::high_resolution_clock::now();
    uint64_t tsc_start = __rdtsc();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    uint64_t tsc_end = __rdtsc();
    auto end = std::chrono::high_resolution_clock::now();

    double ns = std::chrono::duration<double, std::nano>(end - start).count();
    double tsc_freq_ghz = (tsc_end - tsc_start) / ns; // cycles per ns
    std::println("tsc {}", tsc_freq_ghz);

    // for each cache & num threads & capacity (cache options)
    // for each trace
    // for each batch
    // put stats
    std::ofstream out("results.json");

    for (auto &trace : traces) {
      size_t siz = trace.get_working_set_size();
      std::vector<CacheRunner> caches;
      for (double prop : cap_prop) {
        size_t cap = ceil(prop * siz);
        for (size_t num_threads : threads_choices) {
          for (auto &[name, maker] : cache_makers)
            caches.push_back(
                CacheRunner{name, maker(cap), secret, num_threads});
        }
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
          std::println("starting cache {} {} {}", cache.get_name(),
                       cache.get_cap(), cache.get_threads());
          QueryStats stats = cache.do_queries(buf);
          nlohmann::json results;
          results["hit_rate"] = 1.0 * stats.hits / stats.queries;
          results["avg_latency_ns"] =
              1.0 * stats.runtime.count() / stats.queries;
          results["throughput_qps"] =
              1.0 * stats.queries / stats.runtime.count() * 1e9;
          std::vector<double> samples_ns;
          for (auto &cycs : stats.samples) {
            samples_ns.push_back(cycs / tsc_freq_ghz);
          }
          results["samples"] = samples_ns;

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

  // TODO: support args
  template <class T, class... Args>
  void add_cache(const std::string &name, Args &&...args) {
    cache_makers.push_back(
        {name, [](size_t cap) { return std::make_unique<T>(cap); }});
  }
  void add_trace(const std::string &name, const fs::path &path,
                 size_t max_blocks = -1) {
    traces.push_back({name, path, max_blocks});
  }
};