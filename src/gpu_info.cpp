#include "gpu_info.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>

#include "ISystem.h"
#include "ISystem2.h"
#include "adlx.h"
#include "commands.h"

namespace {

// Power limits are only recorded when positive: a 0 W reading is a driver
// "not applicable" sentinel, not a real limit. The highest value seen wins so
// that the range fallbacks cannot downgrade a real reading.
void setPowerCap(StaticInfo& info, int watts) {
  if (watts <= 0) {
    return;
  }
  if (!info.powerCapKnown || watts > info.powerCapWatts) {
    info.powerCapKnown = true;
    info.powerCapWatts = watts;
  }
}

// Reads every metric the driver reports from a single metrics object. Each
// read carries its own validity flag: a capability query can pass and the
// getter still fail at runtime (e.g. the driver deasserts the sensor), so a
// failed read degrades that field to N/A instead of failing the sample.
void readMetricsFields(IADLXGPUMetrics* metrics, const Capabilities& caps,
                       Sample& sample) {
  if (caps.usage) {
    sample.usageOk = ADLX_SUCCEEDED(metrics->GPUUsage(&sample.usage));
  }
  if (caps.temp) {
    sample.tempOk = ADLX_SUCCEEDED(metrics->GPUTemperature(&sample.temp));
  }
  if (caps.hotspot) {
    sample.hotspotOk =
        ADLX_SUCCEEDED(metrics->GPUHotspotTemperature(&sample.hotspot));
  }
  if (caps.power) {
    sample.powerOk = ADLX_SUCCEEDED(metrics->GPUPower(&sample.power));
  }
  if (caps.boardPower) {
    sample.boardPowerOk =
        ADLX_SUCCEEDED(metrics->GPUTotalBoardPower(&sample.boardPower));
  }
  if (caps.fan) {
    sample.fanOk = ADLX_SUCCEEDED(metrics->GPUFanSpeed(&sample.fanRpm));
  }
  if (caps.sclk) {
    sample.sclkOk = ADLX_SUCCEEDED(metrics->GPUClockSpeed(&sample.sclkMhz));
  }
  if (caps.mclk) {
    sample.mclkOk = ADLX_SUCCEEDED(metrics->GPUVRAMClockSpeed(&sample.mclkMhz));
  }
  if (caps.vram) {
    sample.vramOk = ADLX_SUCCEEDED(metrics->GPUVRAM(&sample.vramUsedMiB));
  }
  if (caps.voltage) {
    sample.voltageOk = ADLX_SUCCEEDED(metrics->GPUVoltage(&sample.voltageMv));
  }

  // RDNA3/4 exposes GPU draw as total-board power; the core-only GPUPower
  // metric is not supported there. Surface board power as the usage reading
  // when it exists so the dashboard's Pwr:Usage is never N/A while a live
  // reading is available (this is also the figure nvidia-smi calls Power).
  if (!sample.powerOk && sample.boardPowerOk) {
    sample.powerOk = true;
    sample.power = sample.boardPower;
  }
}

}  // namespace

void readStaticInfo(AdlxSession& session, IADLXGPU* gpu, StaticInfo& info) {
  const char* s = nullptr;
  if (ADLX_SUCCEEDED(gpu->Name(&s)) && s != nullptr) {
    info.name = s;
  }
  if (ADLX_SUCCEEDED(gpu->VendorId(&s)) && s != nullptr) {
    info.vendorId = s;
    info.vendor = gpuVendorName(s);
  }
  if (ADLX_SUCCEEDED(gpu->DeviceId(&s)) && s != nullptr) {
    info.deviceId = s;
  }

  // PCI bus location. Formatted as "bus:device.function" (lspci style) so the
  // dashboard column matches the identity nvidia-smi shows in its Bus-Id
  // column; the chip's device id above remains available separately. The ADL
  // bridge is a singleton ADLX exposes next to the system services.
  IADLMapping* mapping = session.mapping();
  if (mapping != nullptr) {
    adlx_int bus = 0;
    adlx_int device = 0;
    adlx_int function = 0;
    if (ADLX_SUCCEEDED(mapping->BdfFromADLXGPU(gpu, &bus, &device, &function))) {
      char buf[16];
      std::snprintf(buf, sizeof(buf), "%02x:%02x.%x", bus, device, function);
      info.pciBusIdKnown = true;
      info.pciBusId = buf;
    }
  }

  ADLX_GPU_TYPE gpuType = GPUTYPE_UNDEFINED;
  if (ADLX_SUCCEEDED(gpu->Type(&gpuType))) {
    info.type = gpuTypeName(gpuType);
  }

  ADLX_ASIC_FAMILY_TYPE asic = ASIC_UNDEFINED;
  if (ADLX_SUCCEEDED(gpu->ASICFamilyType(&asic))) {
    info.asicFamily = gpuAsicFamilyName(asic);
  }

  if (ADLX_SUCCEEDED(gpu->PNPString(&s)) && s != nullptr) {
    info.pnpString = s;
  }
  if (ADLX_SUCCEEDED(gpu->DriverPath(&s)) && s != nullptr) {
    info.driverPath = s;
  }

  adlx_bool isExternal = 0;
  if (ADLX_SUCCEEDED(gpu->IsExternal(&isExternal))) {
    info.isExternal = (isExternal != 0);
  }

  // Total VRAM. IADLXGPU::TotalVRAM is the authoritative source and reports MB.
  // GetGPUVRAMRange is deliberately *not* used here: its maximum is the
  // supported-range bound for a metric, which is not the board's capacity and
  // can be zero or unrelated to it.
  adlx_uint vramMB = 0;
  if (ADLX_SUCCEEDED(gpu->TotalVRAM(&vramMB)) && vramMB > 0) {
    info.totalVramKnown = true;
    info.totalVramMiB = vramMB;
  }

  // Clock ceilings come from the upper bound of the supported metric ranges.
  IADLXGPUMetricsSupportPtr support;
  if (ADLX_SUCCEEDED(
          session.perfMonitoring()->GetSupportedGPUMetrics(gpu, &support))) {
    adlx_int vmin = 0;
    adlx_int vmax = 0;
    if (ADLX_SUCCEEDED(support->GetGPUClockSpeedRange(&vmin, &vmax)) &&
        vmax > 0) {
      info.maxCoreClockKnown = true;
      info.maxCoreClockMHz = vmax;
    }
    if (ADLX_SUCCEEDED(support->GetGPUVRAMClockSpeedRange(&vmin, &vmax)) &&
        vmax > 0) {
      info.maxMemClockKnown = true;
      info.maxMemClockMHz = vmax;
    }

    // Total board power is a metric (not a tunable): it is consumed per second
    // on every GPU, so its supported range exists even on parts without manual
    // power tuning. Fall back to its ceiling when no explicit limit applies.
    if (ADLX_SUCCEEDED(support->GetGPUTotalBoardPowerRange(&vmin, &vmax)) &&
        vmax > 0) {
      setPowerCap(info, vmax);
    }
  }

  // Driver versions live on the versioned IADLXGPU2 interface. IADLXGPU is the
  // unversioned handle ADLX hands out, so the upcast has to be requested
  // explicitly; the smart pointer does it with QueryInterface and gives us
  // nullptr on older drivers that predate the interface.
  IADLXGPU2Ptr gpu2(gpu);
  if (gpu2 != nullptr) {
    const char* version = nullptr;
    if (ADLX_SUCCEEDED(gpu2->DriverVersion(&version)) && version != nullptr) {
      info.driverVersionKnown = true;
      info.driverVersion = version;
    }
    version = nullptr;
    if (ADLX_SUCCEEDED(gpu2->AMDWindowsDriverVersion(&version)) &&
        version != nullptr) {
      info.windowsDriverVersionKnown = true;
      info.windowsDriverVersion = version;
    }
  }

  // Configured board power limit, in watts.
  //
  // Three sources, tried in order of fidelity:
  //   1. GetPowerLimit            - the limit actually configured
  //   2. GetPowerLimitRange.max   - board limit on drivers that reject the
  //                                 scalar read (e.g. under SmartShift)
  //   3. GPUTotalBoardPowerRange  - total-board-power metric range ceiling,
  //                                 available even without manual power tuning
  setPowerCap(info, 0);

  IADLXInterface* raw = nullptr;
  if (session.gputuning() != nullptr &&
      ADLX_SUCCEEDED(session.gputuning()->GetManualPowerTuning(gpu, &raw)) &&
      raw != nullptr) {
    // Passing the raw base-class pointer selects the smart pointer's templated
    // constructor, which resolves IADLXManualPowerTuning via QueryInterface and
    // owns that fresh reference. The handed-off `raw` reference is ours too
    // and is released separately, so nothing leaks and nothing double-frees.
    IADLXManualPowerTuningPtr powerTuning(raw);
    adlx_int watts = 0;
    if (powerTuning != nullptr &&
        ADLX_SUCCEEDED(powerTuning->GetPowerLimit(&watts)) && watts > 0) {
      setPowerCap(info, watts);
    } else {
      ADLX_IntRange range;
      if (powerTuning != nullptr &&
          ADLX_SUCCEEDED(powerTuning->GetPowerLimitRange(&range)) &&
          range.maxValue > 0) {
        setPowerCap(info, range.maxValue);
      }
    }
    raw->Release();
  }

  if (!info.powerCapKnown) {
    // Neither the tuning limit nor the board-power range was available. The
    // per-second power draw may still be supported, but the *cap* is gone;
    // leave it N/A rather than guessing.
    info.powerCapWatts = 0;
  }
}

Capabilities readCapabilities(AdlxSession& session, IADLXGPU* gpu) {
  Capabilities caps;
  IADLXGPUMetricsSupportPtr support;
  if (ADLX_FAILED(session.perfMonitoring()->GetSupportedGPUMetrics(gpu, &support))) {
    return caps;
  }
  if (support == nullptr) {
    return caps;
  }

  adlx_bool flag = 0;

#define ADLX_QUERY_CAP(field, method)             \
  do {                                            \
    flag = 0;                                     \
    if (ADLX_SUCCEEDED(support->method(&flag))) { \
      caps.field = (flag != 0);                   \
    }                                             \
  } while (0)

  ADLX_QUERY_CAP(usage,      IsSupportedGPUUsage);
  ADLX_QUERY_CAP(temp,       IsSupportedGPUTemperature);
  ADLX_QUERY_CAP(hotspot,    IsSupportedGPUHotspotTemperature);
  ADLX_QUERY_CAP(power,      IsSupportedGPUPower);
  ADLX_QUERY_CAP(boardPower, IsSupportedGPUTotalBoardPower);
  ADLX_QUERY_CAP(fan,        IsSupportedGPUFanSpeed);
  ADLX_QUERY_CAP(sclk,       IsSupportedGPUClockSpeed);
  ADLX_QUERY_CAP(mclk,       IsSupportedGPUVRAMClockSpeed);
  ADLX_QUERY_CAP(vram,       IsSupportedGPUVRAM);
  ADLX_QUERY_CAP(voltage,    IsSupportedGPUVoltage);

#undef ADLX_QUERY_CAP

  return caps;
}

bool readSample(AdlxSession& session, IADLXGPU* gpu, const Capabilities& caps,
                Sample& sample) {
  IADLXGPUMetricsPtr metrics;
  if (ADLX_FAILED(session.perfMonitoring()->GetCurrentGPUMetrics(gpu, &metrics)) ||
      metrics == nullptr) {
    return false;
  }

  metrics->TimeStamp(&sample.timestampMs);
  readMetricsFields(metrics, caps, sample);
  return true;
}

bool readWindowedSample(AdlxSession& session, IADLXGPU* gpu,
                        const Capabilities& caps, Sample& sample,
                        int windowMs) {
  if (windowMs <= 0) {
    return readSample(session, gpu, caps, sample);
  }

  // An instantaneous GetCurrentGPUMetrics acquisition reflects a single driver
  // tick, so usage can come out as 0% and the power draw can be missed. The
  // history buffer is what nvidia-smi-style averages are meant to use: start
  // tracking, let the driver accumulate a window, then average the utilization
  // and power across it. Falls back to the one-shot read when tracking is not
  // available.
  IADLXPerformanceMonitoringServices* monitoring = session.perfMonitoring();
  bool started = ADLX_SUCCEEDED(monitoring->StartPerformanceMetricsTracking());
  if (!started) {
    return readSample(session, gpu, caps, sample);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(windowMs));

  IADLXGPUMetricsListPtr history;
  ADLX_RESULT res =
      monitoring->GetGPUMetricsHistory(gpu, windowMs, 0, &history);
  if (ADLX_FAILED(res) || history == nullptr || history->Empty()) {
    monitoring->StopPerformanceMetricsTracking();
    return readSample(session, gpu, caps, sample);
  }

  // Average usage and power over every sample in the window; keep the newest
  // sample for the sensors (temperature, fan, clocks, VRAM) and the timestamp.
  Sample newest;
  IADLXGPUMetricsPtr item;
  int usageReads = 0;
  int powerReads = 0;
  for (adlx_uint i = history->Begin(); i != history->End(); ++i) {
    if (ADLX_FAILED(history->At(i, &item)) || item == nullptr) {
      continue;
    }
    Sample current;
    readMetricsFields(item, caps, current);
    item->TimeStamp(&current.timestampMs);
    if (current.usageOk) {
      sample.usage += current.usage;
      ++usageReads;
    }
    if (current.powerOk) {
      sample.power += current.power;
      ++powerReads;
    }
    newest = current;
  }

  if (usageReads > 0) {
    sample.usageOk = true;
    sample.usage /= usageReads;
  }
  if (powerReads > 0) {
    sample.powerOk = true;
    sample.power /= powerReads;
  }

  // Carry over the momentary sensor fields from the newest history sample.
  sample.tempOk = newest.tempOk;
  sample.temp = newest.temp;
  sample.hotspotOk = newest.hotspotOk;
  sample.hotspot = newest.hotspot;
  sample.boardPowerOk = newest.boardPowerOk;
  sample.boardPower = newest.boardPower;
  sample.fanOk = newest.fanOk;
  sample.fanRpm = newest.fanRpm;
  sample.sclkOk = newest.sclkOk;
  sample.sclkMhz = newest.sclkMhz;
  sample.mclkOk = newest.mclkOk;
  sample.mclkMhz = newest.mclkMhz;
  sample.vramOk = newest.vramOk;
  sample.vramUsedMiB = newest.vramUsedMiB;
  sample.voltageOk = newest.voltageOk;
  sample.voltageMv = newest.voltageMv;
  sample.timestampMs = newest.timestampMs;

  monitoring->StopPerformanceMetricsTracking();
  return true;
}
