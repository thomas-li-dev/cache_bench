#include "bad_cache.h"
#include "bench.h"
#include "fifo.h"
#include "lru.h"
#include "twi_trace.h"
#include <fcntl.h>

const std::string twi_trace_location = "twi_traces/";
int main() {
  Bench b;
  b.add_cache<FIFO>("FIFO", 1 << 10);
  b.add_cache<LRU>("LRU", 1 << 10);
  b.add_cache<BadCache>("BadCache");
  b.add_trace<TwiTrace>("twitter", twi_trace_location, 1);
  b.run();
}
