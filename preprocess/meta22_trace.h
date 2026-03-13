#include "itrace.h"
#include <cassert>
#include <fcntl.h>
#include <filesystem>
#include <print>
#include <sys/mman.h>
#include <unistd.h>

/*
202206

Those are traces captured for 5 consecutive days from a Meta's key-value cache
cluster consisting of 500 hosts Each host uses (roughly) 42 GB of DRAM and 930
GB of SSD for caching. The open-source traces were merged from multiple hosts
and the effective sampling ratio of is around 1/100.

    key: anonymized requested object ID
    op: operation, GET or SET
    size: the size of the object, could be 0 if it is a cache miss
    op_count: number of operations in the current second
    key_size: size of the object ID

*/
// 5 parts
namespace fs = std::filesystem;
class Meta22Trace : public ITrace {
private:
  const fs::path path;
  fs::directory_iterator next_trace_itr;
  std::string_view current_trace_suffix;

  // these are just used for unmapping.
  void *current_trace_addr = nullptr;
  size_t current_trace_size = 0;
  void *mmap_trace_part(const fs::directory_entry &trace) {
    auto size = fs::file_size(trace.path());
    int fd = open(trace.path().c_str(), O_RDONLY);
    assert(fd != -1);
    void *trace_addr = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    assert(trace_addr != MAP_FAILED);
    close(fd);
    return trace_addr;
  }
  std::optional<cache_key_t> parse_line(std::string_view s) {
    int which = 0;

    uint64_t h = 0;
    bool skip = 0;

    for (size_t k = 0, st = 0; k <= s.size(); k++) {
      if (k == s.size() || s[k] == ',') {
        std::string_view t = s.substr(st, k - st);
        if (which == 0) {
          // compute a simple rolling hash of the string.
          // trivial to hack but should be fine for the trace.
          h = 0;
          for (char c : t)
            h = h * 31 + c;
        } else if (which == 1) {
          // only GET op
          if (!t.starts_with("GET"))
            skip = 1;
        }
        which++;
        st = k + 1;
      }
    }
    if (which != 5 || skip)
      return {};
    return h;
  };
  std::optional<cache_key_t> next_key() override {
    while (true) {
      if (current_trace_suffix.empty()) {
        // default directory is end iterator (least weird cpp design)
        if (next_trace_itr == fs::directory_iterator{})
          return {};
        if (current_trace_addr)
          munmap(current_trace_addr, current_trace_size);

        auto next_trace = *next_trace_itr++;
        current_trace_addr = mmap_trace_part(next_trace);
        current_trace_size = next_trace.file_size();
        current_trace_suffix = {(const char *)current_trace_addr,
                                next_trace.file_size()};
      }
      auto end = current_trace_suffix.find_first_of('\n');
      if (end == std::string_view::npos) {
        // skip to the next trace.
        current_trace_suffix = {};
        continue;
      }
      std::string_view line = current_trace_suffix.substr(0, end);
      current_trace_suffix.remove_prefix(end + 1);
      auto k = parse_line(line);
      if (k) {
        return k;
      }
    }
  }

public:
  // generally we assume that the path contains the whole trace files.
  // don't do much error checking.
  Meta22Trace(const fs::path &path)
      : path(path), next_trace_itr(fs::directory_iterator{path}) {}
  ~Meta22Trace() {
    if (current_trace_addr)
      munmap(current_trace_addr, current_trace_size);
  }

  void reset() override {
    if (current_trace_addr)
      munmap(current_trace_addr, current_trace_size);
    current_trace_addr = nullptr;
    current_trace_size = 0;
    current_trace_suffix = {};
    next_trace_itr = fs::directory_iterator(path);
  }
  // similar interface to read syscall
  size_t next_buf(std::vector<cache_key_t> &buf) override {
    for (size_t i = 0; i < buf.size(); i++) {
      auto k = next_key();
      if (k) {
        buf[i] = *k;
      } else
        return i;
    }
    return buf.size();
  }
};