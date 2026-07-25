#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace diskwatch {

struct DiskStats {
  std::string device;
  uint64_t reads_completed     = 0;
  uint64_t reads_merged        = 0;
  uint64_t read_sectors        = 0;
  uint64_t read_time_ms        = 0;
  uint64_t writes_completed    = 0;
  uint64_t writes_merged       = 0;
  uint64_t write_sectors       = 0;
  uint64_t write_time_ms       = 0;
  uint64_t io_in_progress      = 0;
  uint64_t io_time_ms          = 0;
  uint64_t weighted_io_time_ms = 0;
  uint64_t discard_completed   = 0;
  uint64_t discard_merged      = 0;
  uint64_t discard_sectors     = 0;
  uint64_t discard_time_ms     = 0;
};

struct DeviceInfo {
  std::string device;
  std::string model;
  uint64_t size_sectors = 0;
  bool rotational = false;  // false = SSD, true = HDD
};

/// Returns true if the device name is a virtual device (ram, loop, sr)
[[maybe_unused]] static bool is_virtual_device(const std::string& dev) {
  return dev.starts_with("ram") || dev.starts_with("loop") || dev == "sr0" ||
         dev.starts_with("zram") || dev.starts_with("dm-");
}

/// Read all disk stats from /proc/diskstats
std::vector<DiskStats> read_diskstats(bool include_virtual = false);

/// Read device info from /sys/block/<dev>/
DeviceInfo read_device_info(const std::string& device);

/// Compute derived values
uint64_t bytes_read(const DiskStats& ds);
uint64_t bytes_written(const DiskStats& ds);
double avg_io_latency_ms(const DiskStats& ds);

} // namespace diskwatch
