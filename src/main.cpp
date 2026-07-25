#include <iostream>
#include <print>
#include <cstdio>
#include <chrono>
#include <thread>
#include <vector>
#include <map>
#include <algorithm>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include "diskwatch/disk_stats.hpp"

using json = nlohmann::json;
using namespace diskwatch;

// ── Helpers ──
std::string fmt_bytes(uint64_t bytes) {
  static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
  double v = static_cast<double>(bytes);
  int u = 0;
  while (v >= 1024.0 && u < 4) { v /= 1024.0; ++u; }
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.1f %s", v, units[u]);
  return buf;
}

std::string fmt_iops(uint64_t ops, double seconds) {
  if (seconds <= 0.0) return "0 IOPS";
  double iops = static_cast<double>(ops) / seconds;
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.1f IOPS", iops);
  return buf;
}

// ── Actions ──
void print_table(const std::vector<DiskStats>& stats, bool human_readable) {
  std::println(std::cout, "{:<12} {:>12} {:>12} {:>12} {:>12} {:>12}",
               "DEVICE", "READS", "WRITES", "READ_B", "WRITE_B", "AVG_LAT");
  std::println(std::cout, "{}", std::string(72, '-'));
  for (const auto& ds : stats) {
    auto info = read_device_info(ds.device);
    std::string dev_label = ds.device;
    if (info.rotational) dev_label += " [HDD]";
    else if (info.model.empty()) {}
    else dev_label += " [SSD]";

    if (human_readable) {
      std::println(std::cout, "{:<12} {:>12} {:>12} {:>12} {:>12} {:>8.1f}ms",
                   ds.device,
                   ds.reads_completed, ds.writes_completed,
                   fmt_bytes(bytes_read(ds)), fmt_bytes(bytes_written(ds)),
                   avg_io_latency_ms(ds));
    } else {
      std::println(std::cout, "{:<12} {:>12} {:>12} {:>12} {:>12} {:>8.1f}ms",
                   ds.device,
                   ds.reads_completed, ds.writes_completed,
                   bytes_read(ds), bytes_written(ds),
                   avg_io_latency_ms(ds));
    }
  }
}

void print_json(const std::vector<DiskStats>& stats) {
  json j = json::array();
  for (const auto& ds : stats) {
    auto info = read_device_info(ds.device);
    j.push_back({
      {"device", ds.device},
      {"reads_completed", ds.reads_completed},
      {"writes_completed", ds.writes_completed},
      {"read_sectors", ds.read_sectors},
      {"write_sectors", ds.write_sectors},
      {"bytes_read", bytes_read(ds)},
      {"bytes_written", bytes_written(ds)},
      {"read_time_ms", ds.read_time_ms},
      {"write_time_ms", ds.write_time_ms},
      {"io_in_progress", ds.io_in_progress},
      {"avg_latency_ms", avg_io_latency_ms(ds)},
      {"rotational", info.rotational},
      {"size_sectors", info.size_sectors},
      {"model", info.model}
    });
  }
  std::println(std::cout, "{}", j.dump(2));
}

void print_summary(const std::vector<DiskStats>& stats) {
  uint64_t total_reads = 0, total_writes = 0;
  uint64_t total_read_bytes = 0, total_write_bytes = 0;

  for (const auto& ds : stats) {
    total_reads += ds.reads_completed;
    total_writes += ds.writes_completed;
    total_read_bytes += bytes_read(ds);
    total_write_bytes += bytes_written(ds);
  }

  std::println(std::cout, "Disk I/O Summary ({} devices):", stats.size());
  std::println(std::cout, "  Total Reads:  {} (read ops) / {} bytes",
               total_reads, fmt_bytes(total_read_bytes));
  std::println(std::cout, "  Total Writes: {} (write ops) / {} bytes",
               total_writes, fmt_bytes(total_write_bytes));
  std::println(std::cout, "  Total I/O:    {} ops / {} bytes",
               total_reads + total_writes,
               fmt_bytes(total_read_bytes + total_write_bytes));
}

void print_watch(const std::vector<DiskStats>& initial, int interval_ms, int count) {
  auto prev = initial;
  int iterations = count > 0 ? count : 999999;

  for (int i = 0; i < iterations; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    auto curr = read_diskstats(false);
    double secs = interval_ms / 1000.0;

    std::println(std::cout, "--- diskstat delta (t={}s) ---", secs * (i + 1));
    std::println(std::cout, "{:<12} {:>12} {:>12} {:>12} {:>12}",
                 "DEVICE", "R_IOPS", "W_IOPS", "R_B/s", "W_B/s");

    // Build lookup maps
    std::map<std::string, DiskStats> prev_map, curr_map;
    for (const auto& p : prev) prev_map[p.device] = p;
    for (const auto& c : curr) curr_map[c.device] = c;

    for (const auto& [dev, c] : curr_map) {
      if (!prev_map.contains(dev)) continue;
      const auto& p = prev_map[dev];

      uint64_t d_reads  = c.reads_completed - p.reads_completed;
      uint64_t d_writes = c.writes_completed - p.writes_completed;
      uint64_t d_rbytes = bytes_read(c) - bytes_read(p);
      uint64_t d_wbytes = bytes_written(c) - bytes_written(p);

      std::println(std::cout, "{:<12} {:>12} {:>12} {:>12} {:>12}",
                   dev,
                   fmt_iops(d_reads, secs), fmt_iops(d_writes, secs),
                   fmt_bytes(static_cast<uint64_t>(d_rbytes / secs)) + "/s",
                   fmt_bytes(static_cast<uint64_t>(d_wbytes / secs)) + "/s");
    }
    std::cout << std::flush;
    prev = curr;
  }
}

// ── Main ──
int main(int argc, char** argv) {
  CLI::App app{"diskwatch — Linux Disk I/O Monitor"};
  app.description("Read and display /proc/diskstats statistics");

  bool json_output = false;
  app.add_flag("--json,-j", json_output, "Output in JSON format");

  bool human = true;
  app.add_flag("--raw,-r", human, "Show raw byte counts (not human-readable)")
    ->capture_default_str();

  bool include_virtual = false;
  app.add_flag("--all,-a", include_virtual, "Include virtual devices (ram, loop, dm-)");

  bool show_summary = false;
  app.add_flag("--summary,-s", show_summary, "Show aggregated summary only");

  int interval = 0;
  app.add_option("--watch,-w", interval, "Watch mode: refresh every N ms")
    ->default_val(0);

  int count = -1;
  app.add_option("--count,-n", count, "Number of iterations (default: infinite)")
    ->default_val(-1);

  CLI11_PARSE(app, argc, argv);

  auto stats = read_diskstats(include_virtual);

  if (stats.empty()) {
    spdlog::error("No disk stats read from /proc/diskstats");
    return 1;
  }

  if (json_output) {
    print_json(stats);
  } else if (show_summary) {
    print_summary(stats);
  } else if (interval > 0) {
    print_watch(stats, interval, count);
  } else {
    print_table(stats, human);
  }

  return 0;
}
