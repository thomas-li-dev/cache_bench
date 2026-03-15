#include "itrace.h"
#include <cassert>
#include <fcntl.h>
#include <filesystem>
#include <sys/mman.h>
#include <unistd.h>

/*
202401

Those are traces captured for 5 consecutive days from a Meta's key-value cache
cluster consisting of 8000 hosts Each host uses (roughly) 42 GB of DRAM and 930
GB of SSD for caching.The open-source traces were merged from multiple hosts and
the effective sampling ratio of is around 1/125.

    op_time: the time of the request
    key: anonymized requested object ID
    key_size: size of the object ID
    op: operation, GET, GET_LEASE, SET, DELETE
    op_count: number of operations in the current second
    size: the size of the object, could be 0 if it is a cache miss
    cache_hits: the number of cache hits
    ttl: time-to-live in seconds
    usecase: identifies the tenant, i.e., application using distributed
key-value cache
    sub_usecase: further categorize the different traffics from the
same usecase, but may be imcomplete or inaccurate
*/
// 9 parts
namespace fs = std::filesystem;
class MetaTrace : public ITrace {
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
        if (which == 1) {
          // compute a simple rolling hash of the string.
          // trivial to hack but should be fine for the trace.
          h = 0;
          for (char c : t)
            h = h * 31 + c;
        } else if (which == 3) {
          // only take GET and GET_LEASE operations
          if (!t.starts_with("GET"))
            skip = 1;
        }
        which++;
        st = k + 1;
      }
    }
    if (which != 10 || skip)
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
  MetaTrace(const fs::path &path)
      : path(path), next_trace_itr(fs::directory_iterator{path}) {}
  ~MetaTrace() {
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