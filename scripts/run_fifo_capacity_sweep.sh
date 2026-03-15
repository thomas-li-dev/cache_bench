#!/bin/bash
# Run single-threaded FIFOSingle capacity sweep benchmark.
# Usage: ./scripts/run_fifo_capacity_sweep.sh [traces...]
#   traces: meta22, meta, twitter (default: meta22)

set -euo pipefail
cd "$(dirname "$0")/.."

TRACES="${@:-meta22}"
RESULTS="results_fifo_capacity_sweep.json"

cp main.cpp main.cpp.bak

cat > main.cpp << 'MAIN_EOF'
#include "bench.h"
#include "fifo_single.h"

const std::string twi_trace_location = "twi_proc/";
const std::string meta_trace_location = "meta_proc/";
const std::string meta22_trace_location = "meta22_proc/";

int main() {
  std::vector<size_t> threads_choices = {1};
  std::vector<double> prop{0.01, 0.025, 0.05, 0.075, 0.1, 0.15, 0.2, 0.3, 0.5};
  std::vector<scale_policy> policies{scale_policy::TRANSFORM_SPACE};

  Bench b(threads_choices, prop, policies);
  b.add_cache<FIFOSingle>("FIFOSingle");

MAIN_EOF

for trace in $TRACES; do
  case "$trace" in
    meta22)  echo "  b.add_trace(\"meta22\", meta22_trace_location);" >> main.cpp ;;
    meta)    echo "  b.add_trace(\"meta\", meta_trace_location);" >> main.cpp ;;
    twitter) echo "  b.add_trace(\"twitter\", twi_trace_location);" >> main.cpp ;;
    *)       echo "Unknown trace: $trace" >&2; exit 1 ;;
  esac
done

cat >> main.cpp << 'MAIN_EOF'
  b.run();
}
MAIN_EOF

echo "=== Building benchmark ==="
make -j$(nproc)

echo "=== Running FIFOSingle capacity sweep ==="
./cache_bench

mv results.json "$RESULTS"
echo "=== Results saved to $RESULTS ==="

mv main.cpp.bak main.cpp
echo "=== main.cpp restored ==="

echo ""
echo "To generate combined plots, run:"
echo "  python3 scripts/plot_sieve_comparison.py --input results_capacity_sweep.json $RESULTS"
