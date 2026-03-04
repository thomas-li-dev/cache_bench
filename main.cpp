#include "bench.h"
#include "fifo.h"
#include "twi_trace.h"
#include <fcntl.h>

const std::string twi_trace_location = "twi_traces/";
int main() {
  Bench b;
  b.add_cache<FIFO>("FIFO", 1 << 14);
  b.add_trace<TwiTrace>("twitter", twi_trace_location, 1);
  b.run();
}
