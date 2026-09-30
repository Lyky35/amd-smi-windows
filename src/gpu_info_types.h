#pragma once

#include <cstdint>
#include <string>

// Pure data types shared by the dashboard view and the ADLX reader. Kept free
// of ADLX includes so the host-side tests can exercise the view logic without
// a Windows SDK on the include path.

// Fields that do not change for the lifetime of the process.
struct StaticInfo {
  std::string name = "N/A";
  std::string vendor = "N/A";
  std::string vendorId = "N/A";
  std::string type = "N/A";
  std::string asicFamily = "N/A";
  std::string pnpString = "N/A";
  std::string driverPath = "N/A";
  std::string deviceId = "N/A";
  bool isExternal = false;

  // PCI location as "bus:device.function" (e.g. "01:00.0"), the way lspci and
  // PCI location identify a GPU. The chip device id is kept separately
  // above in `deviceId`; the bus id requires an ADLX BDF query and can be
  // absent on bridged/virtual setups.
  bool pciBusIdKnown = false;
  std::string pciBusId;

  // Total board VRAM, as reported by IADLXGPU::TotalVRAM (MB, not bytes).
  bool totalVramKnown = false;
  long long totalVramMiB = 0;

  bool maxCoreClockKnown = false;
  long long maxCoreClockMHz = 0;

  bool maxMemClockKnown = false;
  long long maxMemClockMHz = 0;

  // Configured board power limit, in watts.
  bool powerCapKnown = false;
  int powerCapWatts = 0;

  // From IADLXGPU2. DriverVersion is the ADLX driver version; the Windows
  // driver is the underlying Adrenalin package version, which is what users
  // see in Device Manager.
  bool driverVersionKnown = false;
  std::string driverVersion;
  bool windowsDriverVersionKnown = false;
  std::string windowsDriverVersion;
};

// Which metrics the driver reports as supported for a given GPU.
struct Capabilities {
  bool usage = false;
  bool temp = false;
  bool hotspot = false;
  bool power = false;
  bool boardPower = false;
  bool fan = false;
  bool sclk = false;
  bool mclk = false;
  bool vram = false;
  bool voltage = false;
};

// One telemetry sample. Every field carries its own validity flag so
// unsupported metrics degrade to "N/A" rather than failing the report.
struct Sample {
  bool usageOk = false;
  double usage = 0;

  bool tempOk = false;
  double temp = 0;

  bool hotspotOk = false;
  double hotspot = 0;

  bool powerOk = false;
  double power = 0;

  bool boardPowerOk = false;
  double boardPower = 0;

  bool fanOk = false;
  int fanRpm = 0;

  bool sclkOk = false;
  int sclkMhz = 0;

  bool mclkOk = false;
  int mclkMhz = 0;

  bool vramOk = false;
  int vramUsedMiB = 0;

  bool voltageOk = false;
  int voltageMv = 0;

  int64_t timestampMs = 0;
};