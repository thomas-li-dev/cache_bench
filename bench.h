// give set of eviction algos, and set of traces
// for each trace, run all eviction algos
// collect metrics
#include "cache_runner.h"
#include "cpu_topology.h"
#include "ext/json.hpp"
#include "trace.h"
#include <chrono>
#include <fstream>
#include <functional>
#include <memory>
#include <cstdio>
#include <vector>

struct BenchEntry {
  std::string name;
  size_t num_threads, cap;
  double cap_prop;
  scale_policy sp;
  std::function<QueryStats(std::span<cache_key_t>)> do_queries;
};

class Bench {
private:
  struct CacheMaker {
    std::string name;
    bool can_multi;
    std::function<BenchEntry(size_t cap, uint64_t secret, size_t num_threads,
                             scale_policy sp, double cap_prop,
                             std::span<const int> cpu_order)>
        make;
  };
  std::vector<CacheMaker> cache_makers;
  std::vector<Trace> traces;
  uint64_t secret = (int64_t)1e11 + 3;

  std::vector<size_t> threads_choices;
  std::vector<double> cap_prop;
  std::vector<scale_policy> scale_policies;

public:
  Bench(const std::vector<size_t> &threads_choices,
        const std::vector<double> &cap_prop,
        const std::vector<scale_policy> &scale_policies =
            {scale_policy::INTERLEAVE})
      : threads_choices(threads_choices), cap_prop(cap_prop),
        scale_policies(scale_policies) {}
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
    printf("tsc %f\n", tsc_freq_ghz);

    // for each cache & num threads & capacity (cache options)
    // for each trace
    // for each batch
    // put stats
    std::ofstream out("results.json");
    auto cpu_order = get_cpu_order();

    for (auto &trace : traces) {
      size_t siz = trace.get_working_set_size();
      std::vector<BenchEntry> caches;
      for (scale_policy sp : scale_policies) {
        for (double prop : cap_prop) {
          for (size_t num_threads : threads_choices) {
            size_t cap_scale =
                sp == scale_policy::TRANSFORM_SPACE ? num_threads : 1;
            size_t cap = ceil(prop * siz * cap_scale);
            for (auto &[name, can_multi, make] : cache_makers)
              if (num_threads == 1 || can_multi)
                caches.push_back(
                    make(cap, secret, num_threads, sp, prop, cpu_order));
          }
        }
      }
      printf("running trace %s\n", trace.get_name().c_str());
      std::vector<cache_key_t> buf;
      for (size_t batch = 0;; batch++) {
        auto start = std::chrono::high_resolution_clock::now();
        trace.next_block(buf);
        auto end = std::chrono::high_resolution_clock::now();
        if (buf.empty())
          break;
        printf("batch %zu start\n", batch);
        printf("loaded queries into buf in %f ms\n",
            std::chrono::duration<double, std::milli>(end - start).count());
        for (auto &entry : caches) {
          printf("starting cache %s %zu %zu\n", entry.name.c_str(), entry.cap,
                       entry.num_threads);
          QueryStats stats = entry.do_queries(buf);
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

          results["cache_name"] = entry.name;
          results["trace_name"] = trace.get_name();
          results["threads"] = entry.num_threads;
          results["batch"] = batch;
          results["capacity"] = entry.cap;
          results["cap_prop"] = entry.cap_prop;
          results["scale_policy"] = scale_policy_name(entry.sp);
          out << results.dump() << "\n";
        }
      }
    }
  }

  template <class T>
  void add_cache(const std::string &name) {
    cache_makers.push_back(
        {name, T::can_multithread(),
         [name](size_t cap, uint64_t secret, size_t num_threads,
                scale_policy sp, double cap_prop,
                std::span<const int> cpu_order) -> BenchEntry {
           auto runner = std::make_shared<CacheRunner<T>>(
               name, cap, secret, num_threads, sp, cpu_order);
           return BenchEntry{
               name, num_threads, runner->get_cap(), cap_prop, sp,
               [r = std::move(runner)](std::span<cache_key_t> buf) mutable {
                 return r->do_queries(buf);
               }};
         }});
  }
  void add_trace(const std::string &name, const fs::path &path,
                 size_t max_blocks = -1) {
    traces.push_back({name, path, max_blocks});
  }
};
