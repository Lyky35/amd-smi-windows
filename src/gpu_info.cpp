#include "gpu_info.h"

#include <algorithm>

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

  // A getter can still fail at runtime even when the capability query passed
  // (e.g. the driver deasserts the sensor), so treat each read independently.
  if (caps.usage) {
    sample.usageOk = ADLX_SUCCEEDED(metrics->GPUUsage(&sample.usage));
  }
  if (caps.temp) {
    sample.tempOk = ADLX_SUCCEEDED(metrics->GPUTemperature(&sample.temp));
  }
  if (caps.hotspot) {
    sample.hotspotOk = ADLX_SUCCEEDED(metrics->GPUHotspotTemperature(&sample.hotspot));
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

  return true;
}
