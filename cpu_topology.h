#pragma once
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <thread>
#include <vector>

// Returns logical CPU IDs ordered so that unique physical cores come first.
// Reads topology from sysfs. Falls back to 0,1,2,... if unavailable.
inline std::vector<int> get_cpu_order() {
  namespace fs = std::filesystem;
  struct CpuInfo {
    int cpu_id, core_id;
  };
  std::vector<CpuInfo> cpus;
  for (auto &entry : fs::directory_iterator("/sys/devices/system/cpu")) {
    auto name = entry.path().filename().string();
    if (!name.starts_with("cpu"))
      continue;
    auto id_str = name.substr(3);
    if (id_str.empty() || !std::isdigit(id_str[0]))
      continue;
    int cpu_id = std::stoi(id_str);
    auto core_path = entry.path() / "topology" / "core_id";
    if (!fs::exists(core_path))
      continue;
    std::ifstream f(core_path);
    int core_id;
    f >> core_id;
    cpus.push_back({cpu_id, core_id});
  }
  if (cpus.empty()) {
    int n = std::thread::hardware_concurrency();
    std::vector<int> fallback(n);
    std::iota(fallback.begin(), fallback.end(), 0);
    return fallback;
  }
  // stable sort by first-seen order of core_id, so first logical CPU per
  // physical core comes before its sibling
  std::ranges::sort(cpus, {}, &CpuInfo::cpu_id);
  std::vector<int> seen_cores;
  std::vector<int> primary, secondary;
  for (auto &[cpu_id, core_id] : cpus) {
    if (std::ranges::find(seen_cores, core_id) == seen_cores.end()) {
      seen_cores.push_back(core_id);
      primary.push_back(cpu_id);
    } else {
      secondary.push_back(cpu_id);
    }
  }
  primary.insert(primary.end(), secondary.begin(), secondary.end());
  return primary;
}
