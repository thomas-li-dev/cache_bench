#include "bad_cache.h"
#include "bench.h"
#include "cache_runner.h"
#include "fifo.h"
#include "lru.h"
#include "sieve.h"

const std::string twi_trace_location = "twi_proc/";
const std::string meta_trace_location = "meta_proc/";
int main() {
  std::vector<size_t> threads_choices = {1, 8};
  std::vector<double> prop{0.1};
  std::vector<scale_policy> policies{
      scale_policy::INTERLEAVE,
  };
  Bench b(threads_choices, prop, policies);
  b.add_cache<FIFO>("FIFO");
  b.add_cache<SIEVE>("SIEVE");
  // b.add_cache<LRU>("lru");
  //  b.add_cache<BadCache>("BadCache");
  // b.add_trace("meta", meta_trace_location, 1);
  b.add_trace("twitter", twi_trace_location, 20);
  b.run();
}
