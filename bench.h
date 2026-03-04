// give set of eviction algos, and set of traces
// for each trace, run all eviction algos
// collect metrics
#include "cache_runner.h"
#include "fifo.h"
#include "trace.h"
#include <memory>
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

public:
  void run() {
    for (auto &[name, trace] : traces) {
      std::println("running trace {}", name);
      int qp = 0;
      trace->run_each_query([&](key key) {
        for (auto &cache : caches) {
          cache.do_query(key);
        }
        if (++qp % 1000000 == 0) {
          std::println("processed {} queries", qp);
          /*
          for (auto &cache : caches)
            cache.print();*/
          // TODO: have proper way to warm up and then measure.
          for (auto &cache : caches)
            cache.reset();
        }
      });
    }
    for (auto &cache : caches)
      cache.print();
  }
  template <class T, class... Args>
  void add_cache(const std::string &name, Args &&...args) {
    caches.push_back(CacheRunner(name, std::make_unique<T>(args...), secret));
  }
  template <class T, class... Args>
  void add_trace(const std::string &name, Args &&...args) {
    traces.push_back({name, std::make_unique<T>(args...)});
  }
};