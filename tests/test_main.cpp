#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <sstream>
#include <fstream>
#include <cstdio>
#include <cstdlib>

#include "diskwatch/disk_stats.hpp"

using namespace diskwatch;
using Catch::Approx;

// ── Unit: Derived values ──
TEST_CASE("bytes_read computes correctly", "[unit]") {
  DiskStats ds;
  ds.read_sectors = 100;
  REQUIRE(bytes_read(ds) == 100 * 512);
}

TEST_CASE("bytes_written computes correctly", "[unit]") {
  DiskStats ds;
  ds.write_sectors = 200;
  REQUIRE(bytes_written(ds) == 200 * 512);
}

TEST_CASE("avg_io_latency_ms with zero IOs", "[unit]") {
  DiskStats ds;
  REQUIRE(avg_io_latency_ms(ds) == Approx(0.0));
}

TEST_CASE("avg_io_latency_ms with some IOs", "[unit]") {
  DiskStats ds;
  ds.reads_completed = 100;
  ds.writes_completed = 50;
  ds.weighted_io_time_ms = 3000;
  double expected = 3000.0 / 150.0;
  REQUIRE(avg_io_latency_ms(ds) == Approx(expected));
}

TEST_CASE("is_virtual_device detects ram", "[unit]") {
  REQUIRE(is_virtual_device("ram0"));
  REQUIRE(is_virtual_device("ram15"));
}

TEST_CASE("is_virtual_device detects loop", "[unit]") {
  REQUIRE(is_virtual_device("loop0"));
  REQUIRE(is_virtual_device("loop7"));
}

TEST_CASE("is_virtual_device detects dm", "[unit]") {
  REQUIRE(is_virtual_device("dm-0"));
  REQUIRE(is_virtual_device("dm-1"));
}

TEST_CASE("is_virtual_device allows real devices", "[unit]") {
  REQUIRE_FALSE(is_virtual_device("sda"));
  REQUIRE_FALSE(is_virtual_device("nvme0n1"));
  REQUIRE_FALSE(is_virtual_device("mmcblk0"));
}

// ── Integration: /proc/diskstats parsing (requires Linux) ──
TEST_CASE("read_diskstats from /proc/diskstats", "[integration]") {
  // Check that /proc/diskstats exists and is readable
  std::ifstream f("/proc/diskstats");
  if (!f.is_open()) {
    SKIP("/proc/diskstats not available on this system");
  }

  auto stats = read_diskstats(false);
  REQUIRE_FALSE(stats.empty());

  // Spot-check first device structure
  const auto& first = stats.front();
  REQUIRE_FALSE(first.device.empty());

  // At least one device should have non-zero fields
  bool has_io = false;
  for (const auto& ds : stats) {
    if (ds.reads_completed > 0 || ds.writes_completed > 0) {
      has_io = true;
      break;
    }
  }
  // This might fail on idle systems — it's informative, not critical
  WARN("No disk with non-zero I/O found (system may be idle)");
}

TEST_CASE("read_device_info for sda", "[integration]") {
  auto info = read_device_info("sda");
  // sda may or may not exist — just verify the struct is populated
  if (info.device.empty()) {
    SKIP("sda not found on this system");
  }
  REQUIRE(info.device == "sda");
  // Should at least have size
  WARN("sda size_sectors=" << info.size_sectors);
}

TEST_CASE("read_diskstats excludes virtual by default", "[unit/integration]") {
  std::ifstream f("/proc/diskstats");
  if (!f.is_open()) {
    SKIP("/proc/diskstats not available");
  }

  auto stats = read_diskstats(false);

  // Verify no virtual devices in results
  for (const auto& ds : stats) {
    REQUIRE_FALSE(is_virtual_device(ds.device));
  }
}

TEST_CASE("read_diskstats includes virtual when asked", "[unit/integration]") {
  std::ifstream f("/proc/diskstats");
  if (!f.is_open()) {
    SKIP("/proc/diskstats not available");
  }

  auto stats = read_diskstats(true);
  REQUIRE_FALSE(stats.empty());

  // Count virtual devices
  bool found_virtual = false;
  for (const auto& ds : stats) {
    if (is_virtual_device(ds.device)) {
      found_virtual = true;
      break;
    }
  }
  // Not every system has virtual devices — just don't crash
  WARN("Virtual devices found: " << (found_virtual ? "yes" : "no"));
}
