#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "commands.h"
#include "gpu_info.h"
#include "output.h"

namespace {

std::string na(const std::string& value, bool ok) {
  return ok ? value : std::string("N/A");
}

void printJson(AdlxSession& session, const std::vector<size_t>& indices,
               const std::vector<Sample>& samples) {
  JsonBuilder json(OutputFormat::Json);
  json.beginObject("");
  json.field("tool", "amd-smi-win");
  json.field("backend", "ADLX");
  json.field("adlx_version", session.version());

  json.beginObject("gpus");
  for (size_t i = 0; i < indices.size(); ++i) {
    const Sample& s = samples[i];
    json.beginObject(std::to_string(indices[i]));
    json.field("timestamp_ms", static_cast<long long>(s.timestampMs));
    json.optionalField("usage_percent", s.usageOk, s.usage);
    json.optionalField("temperature_c", s.tempOk, s.temp);
    json.optionalField("hotspot_temperature_c", s.hotspotOk, s.hotspot);
    json.optionalField("power_w", s.powerOk, s.power);
    json.optionalField("board_power_w", s.boardPowerOk, s.boardPower);
    json.optionalField("fan_rpm", s.fanOk, (double)s.fanRpm);
    json.optionalField("sclk_mhz", s.sclkOk, (double)s.sclkMhz);
    json.optionalField("mclk_mhz", s.mclkOk, (double)s.mclkMhz);
    json.optionalField("vram_used_mib", s.vramOk, (double)s.vramUsedMiB);
    json.optionalField("voltage_mv", s.voltageOk, (double)s.voltageMv);
    json.endObject();
  }
  json.endObject();

  json.endObject();
  json.finish();
}

}  // namespace

int cmdMetric(AdlxSession& session, const Options& opts) {
  std::vector<size_t> indices;
  std::string error;
  if (!resolveGpuSelection(opts, session, indices, error)) {
    std::fputs((error + "\n").c_str(), stderr);
    return 1;
  }

  std::vector<Capabilities> caps;
  caps.reserve(indices.size());
  for (size_t idx : indices) {
    caps.push_back(readCapabilities(session, session.gpus()[idx]));
  }

  const int iterations = (opts.intervalMs > 0) ? opts.iterations : 1;
  const bool repeat = iterations > 1 || opts.intervalMs > 0;

  for (int iteration = 0; iteration < iterations; ++iteration) {
    if (repeat && iteration > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(opts.intervalMs));
    }

    std::vector<Sample> samples(indices.size());
    bool anyOk = false;
    for (size_t i = 0; i < indices.size(); ++i) {
      anyOk |= readSample(session, session.gpus()[indices[i]], caps[i], samples[i]);
    }

    if (!anyOk) {
      std::fputs(
          "error: the driver returned no metrics. On Windows, performance "
          "monitoring requires the AMD Software: Adrenalin Edition driver and "
          "usually an elevated session.\n",
          stderr);
      return 1;
    }

    if (opts.format == OutputFormat::Json) {
      printJson(session, indices, samples);
    } else {
      Table table;
      table.headers = {"GPU", "UTIL%", "TEMP", "HOTSPOT", "POWER", "BOARD PWR",
                       "FAN", "SCLK", "MCLK", "VRAM USED", "VOLT"};
      for (size_t i = 0; i < indices.size(); ++i) {
        const Sample& s = samples[i];
        table.add({
            std::to_string(indices[i]),
            na(formatDouble(s.usage) + "%", s.usageOk),
            na(formatDouble(s.temp) + "C", s.tempOk),
            na(formatDouble(s.hotspot) + "C", s.hotspotOk),
            na(formatDouble(s.power) + "W", s.powerOk),
            na(formatDouble(s.boardPower) + "W", s.boardPowerOk),
            na(std::to_string(s.fanRpm) + "RPM", s.fanOk),
            na(std::to_string(s.sclkMhz) + "MHz", s.sclkOk),
            na(std::to_string(s.mclkMhz) + "MHz", s.mclkOk),
            na(std::to_string(s.vramUsedMiB) + "MiB", s.vramOk),
            na(std::to_string(s.voltageMv) + "mV", s.voltageOk),
        });
      }
      render(table, opts.format);
    }

    // Clear the screen between iterations so watch mode stays readable.
    if (repeat && iteration + 1 < iterations && opts.format == OutputFormat::Table) {
      std::printf("\x1b[H\x1b[J");
    }
  }

  return 0;
}
