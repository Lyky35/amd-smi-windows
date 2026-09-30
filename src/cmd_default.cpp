// The default (no-subcommand) view: the all-in-one dashboard.
//
// ADLX does not expose a PCI bus address, a P-state, an ECC field or a
// persistence-mode notion, so those are omitted rather than faked. ADLX does
// expose the configured board power limit through IGPUManualPowerTuning.

#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "commands.h"
#include "default_view.h"
#include "gpu_info.h"
#include "output.h"

namespace {

// Uses the global (C) time functions because libc++ on Windows does not expose
// strftime/time_t through std::.
std::string currentTimestamp() {
  time_t now = time(nullptr);
  struct tm local {};
#if defined(_WIN32)
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  char buf[64];
  if (strftime(buf, sizeof(buf), "%a %b %e %H:%M:%S %Y", &local) == 0) {
    return std::string();
  }
  return std::string(buf);
}

void printJson(AdlxSession& session, const std::vector<size_t>& indices,
               const std::vector<StaticInfo>& infos,
               const std::vector<Sample>& samples) {
  JsonBuilder json(OutputFormat::Json);
  json.beginObject("");
  json.field("tool", "amd-smi-win");
  json.field("backend", "ADLX");
  json.field("adlx_version", session.version());
  json.field("timestamp", currentTimestamp());
  json.field("gpu_count", static_cast<long long>(indices.size()));

  json.beginObject("gpus");
  for (size_t i = 0; i < indices.size(); ++i) {
    const StaticInfo& st = infos[i];
    const Sample& sm = samples[i];
    json.beginObject(std::to_string(indices[i]));
    json.field("name", st.name);
    json.field("vendor", st.vendor);
    json.field("device_id", st.deviceId);
    if (st.pciBusIdKnown) {
      json.field("pci_bus_id", st.pciBusId);
    }
    json.field("asic_family", st.asicFamily);
    json.optionalField("vram_total_mib", st.totalVramKnown, (double)st.totalVramMiB);
    if (st.driverVersionKnown) {
      json.field("driver_version", st.driverVersion);
    }
    if (st.windowsDriverVersionKnown) {
      json.field("windows_driver_version", st.windowsDriverVersion);
    }
    json.optionalField("power_cap_watts", st.powerCapKnown, (double)st.powerCapWatts);
    json.optionalField("temperature_c", sm.tempOk, sm.temp);
    json.optionalField("fan_rpm", sm.fanOk, (double)sm.fanRpm);
    json.optionalField("power_w", sm.powerOk, sm.power);
    json.optionalField("vram_used_mib", sm.vramOk, (double)sm.vramUsedMiB);
    json.optionalField("usage_percent", sm.usageOk, sm.usage);
    json.optionalField("sclk_mhz", sm.sclkOk, (double)sm.sclkMhz);
    json.optionalField("mclk_mhz", sm.mclkOk, (double)sm.mclkMhz);
    json.endObject();
  }
  json.endObject();

  json.endObject();
  json.finish();
}

}  // namespace

int cmdDefault(AdlxSession& session, const Options& opts) {
  std::vector<size_t> indices;
  std::string error;
  if (!resolveGpuSelection(opts, session, indices, error)) {
    std::fputs((error + "\n").c_str(), stderr);
    return 1;
  }

  std::vector<StaticInfo> infos(indices.size());
  std::vector<Sample> samples(indices.size());
  std::vector<Capabilities> caps(indices.size());

  for (size_t i = 0; i < indices.size(); ++i) {
    IADLXGPU* gpu = session.gpus()[indices[i]];
    readStaticInfo(session, gpu, infos[i]);
    caps[i] = readCapabilities(session, gpu);
    int averaged = 0;
    readWindowedSample(session, gpu, caps[i], samples[i], 1000, &averaged);
    // A dashboard utilization of 0% means one of two very different things: the
    // GPU really is idle, or the driver returned no history to average over and
    // a single instantaneous tick was reported. The sample count tells them
    // apart, which is the difference between a real reading and a missed one.
    if (opts.verbose) {
      std::fprintf(stderr,
                   "gpu %zu: utilization averaged over %d history sample(s)\n",
                   indices[i], averaged);
    }
  }

  if (opts.format == OutputFormat::Json) {
    printJson(session, indices, infos, samples);
    return 0;
  }

  // -1 so the table still fits within a console that is exactly one column
  // narrower than the table wants (terminals that keep a scrollbar).
  int width = opts.width > 0 ? opts.width : detectConsoleWidth();
  DefaultView view(width > 0 ? width - 1 : 0);

  std::printf("%s\n", view.banner(infos, session.version()).c_str());
  std::printf("%s\n", view.topRule().c_str());
  std::printf("%s\n", view.staticHeader().c_str());
  std::printf("%s\n", view.metricHeader().c_str());
  std::printf("%s\n", view.bottomRule().c_str());

  for (size_t i = 0; i < indices.size(); ++i) {
    std::printf("%s\n", view.staticRow((int)indices[i], infos[i]).c_str());
    std::printf("%s\n", view.metricRow(samples[i], infos[i]).c_str());
  }

  std::printf("%s\n", view.bottomRule().c_str());
  std::printf(
      "Legend: Pwr:Usage/Cap is the average GPU board draw over the last second "
      "against the configured power limit. Metrics a given GPU does not support "
      "are shown as N/A.\n"
      "Columns that do not fit the console are dropped; resize the window (or\n"
      "set COLUMNS) for the full layout. Run 'amd-smi --help' for the complete\n"
      "command set, and 'static', 'metric' for detail.\n");

  return 0;
}