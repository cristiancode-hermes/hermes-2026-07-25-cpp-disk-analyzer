#include "diskwatch/disk_stats.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace diskwatch {

std::vector<DiskStats> read_diskstats(bool include_virtual) {
  std::ifstream f("/proc/diskstats");
  if (!f.is_open()) return {};

  std::vector<DiskStats> stats;
  std::string line;
  while (std::getline(f, line)) {
    std::istringstream ss(line);
    DiskStats ds;
    int major = 0, minor = 0;
    if (!(ss >> major >> minor >> ds.device)) continue;
    if (!include_virtual && is_virtual_device(ds.device)) continue;

    ss >> ds.reads_completed >> ds.reads_merged >> ds.read_sectors >> ds.read_time_ms
       >> ds.writes_completed >> ds.writes_merged >> ds.write_sectors >> ds.write_time_ms
       >> ds.io_in_progress >> ds.io_time_ms >> ds.weighted_io_time_ms;

    // Optional discard fields (kernel 5.x+)
    if (ss >> ds.discard_completed) {
      ss >> ds.discard_merged >> ds.discard_sectors >> ds.discard_time_ms;
    }

    stats.push_back(std::move(ds));
  }
  return stats;
}

DeviceInfo read_device_info(const std::string& device) {
  DeviceInfo info{device};
  std::string base = "/sys/block/" + device;
  if (!std::filesystem::exists(base)) return info;

  // Read size
  if (std::ifstream size_f(base + "/size"); size_f) {
    size_f >> info.size_sectors;
  }

  // Read rotational
  if (std::ifstream rot_f(base + "/queue/rotational"); rot_f) {
    std::string val;
    rot_f >> val;
    info.rotational = (val == "1");
  }

  // Read model
  if (std::ifstream model_f(base + "/device/model"); model_f) {
    std::getline(model_f, info.model);
    // Trim whitespace
    info.model.erase(info.model.find_last_not_of(" \t\n\r") + 1);
  }

  return info;
}

uint64_t bytes_read(const DiskStats& ds) {
  return ds.read_sectors * 512;
}

uint64_t bytes_written(const DiskStats& ds) {
  return ds.write_sectors * 512;
}

double avg_io_latency_ms(const DiskStats& ds) {
  uint64_t total_ios = ds.reads_completed + ds.writes_completed;
  if (total_ios == 0) return 0.0;
  return static_cast<double>(ds.weighted_io_time_ms) / static_cast<double>(total_ios);
}

} // namespace diskwatch
