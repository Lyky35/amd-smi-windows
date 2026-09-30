#include <cstdio>
#include <string>
#include <vector>

#include "commands.h"
#include "gpu_info.h"
#include "output.h"

namespace {

void printJson(AdlxSession& session, const std::vector<size_t>& indices,
               const std::vector<StaticInfo>& infos) {
  JsonBuilder json(OutputFormat::Json);
  json.beginObject("");
  json.field("tool", "amd-smi-win");
  json.field("backend", "ADLX");
  json.field("adlx_version", session.version());

  json.beginObject("gpus");
  for (size_t i = 0; i < indices.size(); ++i) {
    const StaticInfo& info = infos[i];
    json.beginObject(std::to_string(indices[i]));
    json.field("name", info.name);
    json.field("vendor", info.vendor);
    json.field("vendor_id", info.vendorId);
    json.field("device_id", info.deviceId);
    json.field("type", info.type);
    json.field("asic_family", info.asicFamily);
    json.field("pnp_string", info.pnpString);
    json.field("driver_path", info.driverPath);
    json.field("external", info.isExternal);
    json.optionalField("vram_total_mib", info.totalVramKnown, (double)info.totalVramMiB);
    if (info.driverVersionKnown) {
      json.field("driver_version", info.driverVersion);
    }
    if (info.windowsDriverVersionKnown) {
      json.field("windows_driver_version", info.windowsDriverVersion);
    }
    json.optionalField("max_core_clock_mhz", info.maxCoreClockKnown, info.maxCoreClockMHz);
    json.optionalField("max_mem_clock_mhz", info.maxMemClockKnown, info.maxMemClockMHz);
    json.optionalField("power_cap_watts", info.powerCapKnown, (double)info.powerCapWatts);
    json.endObject();
  }
  json.endObject();

  json.endObject();
  json.finish();
}

}  // namespace

int cmdStatic(AdlxSession& session, const Options& opts) {
  std::vector<size_t> indices;
  std::string error;
  if (!resolveGpuSelection(opts, session, indices, error)) {
    std::fputs((error + "\n").c_str(), stderr);
    return 1;
  }

  std::vector<StaticInfo> infos;
  infos.reserve(indices.size());
  for (size_t idx : indices) {
    StaticInfo info;
    readStaticInfo(session, session.gpus()[idx], info);
    infos.push_back(std::move(info));
  }

  if (opts.format == OutputFormat::Json) {
    printJson(session, indices, infos);
    return 0;
  }

  Table table;
  table.headers = {"GPU", "NAME", "VENDOR", "TYPE", "ASIC", "VRAM TOTAL",
                   "MAX SCLK", "MAX MCLK", "POWER CAP", "EXTERNAL"};
  for (size_t i = 0; i < indices.size(); ++i) {
    const StaticInfo& info = infos[i];
    table.add({
        std::to_string(indices[i]),
        info.name,
        info.vendor,
        info.type,
        info.asicFamily,
        info.totalVramKnown ? std::to_string(info.totalVramMiB) + " MiB" : "N/A",
        info.maxCoreClockKnown ? std::to_string(info.maxCoreClockMHz) + " MHz" : "N/A",
        info.maxMemClockKnown ? std::to_string(info.maxMemClockMHz) + " MHz" : "N/A",
        info.powerCapKnown ? std::to_string(info.powerCapWatts) + " W" : "N/A",
        info.isExternal ? "yes" : "no",
    });
  }
  render(table, opts.format);

  if (opts.verbose) {
    std::fprintf(stderr, "\nADLX %s\n", session.version().c_str());
    for (size_t i = 0; i < indices.size(); ++i) {
      std::fprintf(stderr, "gpu%zu pnp:  %s\n", indices[i], infos[i].pnpString.c_str());
      std::fprintf(stderr, "gpu%zu drv:  %s\n", indices[i], infos[i].driverPath.c_str());
    }
  }

  return 0;
}
