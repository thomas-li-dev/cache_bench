#include "bad_cache.h"
#include "bench.h"
#include "cache_runner.h"
#include "fifo.h"
#include "fifo_batch.h"
#include "lru.h"
#include "sieve.h"
#include "sieve_bit.h"
#include "sieve_naive.h"
#include "sieve_single.h"

const std::string twi_trace_location = "twi_proc/";
const std::string meta_trace_location = "meta_proc/";
const std::string meta22_trace_location = "meta22_proc/";
int main() {
  std::vector<size_t> threads_choices = {1};
  std::vector<double> prop{0.01};
  std::vector<scale_policy> policies{
      scale_policy::INTERLEAVE,
  };
  Bench b(threads_choices, prop, policies);
  // b.add_cache<FIFO>("FIFO");
  //  b.add_cache<SIEVENaive>("SIEVENaive");
  // b.add_cache<SIEVE>("SIEVE");
  b.add_cache<SIEVESingle>("SIEVESingle");
  b.add_cache<SIEVEBit>("SIEVEBit");
  //  b.add_cache<LRU>("lru");
  //   b.add_cache<BadCache>("BadCache");
  // b.add_trace("meta", meta_trace_location, 100);
  b.add_trace("meta22", meta22_trace_location, 1);
  // b.add_trace("twitter", twi_trace_location, 1);
  b.run();
}
