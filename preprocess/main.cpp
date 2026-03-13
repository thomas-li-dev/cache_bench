#include "ext/hyperloglog.hpp"
#include "ext/json.hpp"
#include "itrace.h"
#include "meta22_trace.h"
#include "meta_trace.h"
#include "trace.h"
#include "twi_trace.h"
#include "types.h"
#include <fstream>
#include <unordered_set>
using namespace std;

// trace dir should only contain the trace files.
int main(int argc, const char **argv) {
  if (argc < 6) {
    std::println(
        "args: trace name, trace dir, out dir, batch size, num batches\n");
    return 1;
  }
  namespace fs = std::filesystem;
  string name = argv[1];
  fs::path trace_dir{argv[2]}, out_dir{argv[3]};
  size_t batch_size = stoull(argv[4]), num_batches = stoull(argv[5]);
  // just assume arguments are reasonable
  // if not, skill issue
  ITrace *t = nullptr;
  if (name == "twi") {
    t = new TwiTrace(trace_dir);
  } else if (name == "meta") {
    t = new MetaTrace(trace_dir);
  } else if (name == "meta22") {
    t = new Meta22Trace(trace_dir);
  } else {
    std::println("invalid trace name\n");
    return 1;
  }
  std::println("making files of {} queries", batch_size);
  hll::HyperLogLog hll(10);
  std::vector<cache_key_t> buf(batch_size);
  for (size_t f = 0; f < num_batches; f++) {
    size_t num = t->next_buf(buf);
    if (!num)
      break;
    std::unordered_set<cache_key_t> unq;
    for (size_t i = 0; i < num; i++) {
      unq.insert(buf[i]);
      // just add this as a "string" of 8 characters?
      hll.add((char *)&buf[i], sizeof(cache_key_t));
    }
    Metadata meta{.uniq_keys = unq.size()};

    std::string file_name = name + to_string(f) + ".trace";
    std::ofstream file;
    fs::path file_path = out_dir / file_name;
    std::println("writing to {} {} queries", file_path.string(), num);
    file.open(file_path, std::ios::binary);
    file.write((char *)&meta, sizeof(Metadata));
    file.write((char *)buf.data(), sizeof(cache_key_t) * num);
  }
  int est = ceil(hll.estimate());

  std::println("estim {}", est);
  nlohmann::json j;
  j["uniq_keys"] = est;
  std::ofstream out(out_dir / "meta.json");
  out << j.dump();

  delete t;
}