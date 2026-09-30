#include "default_view.h"

#include <cstring>
#include <ctime>
#include <map>

#include "layout.h"
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
  char buffer[64];
  if (strftime(buffer, sizeof(buffer), "%a %b %e %H:%M:%S %Y", &local) == 0) {
    return std::string();
  }
  return std::string(buffer);
}

}  // namespace

DefaultView::DefaultView(int terminalWidth)
    : m_columns(fitColumns(defaultColumns(), terminalWidth)) {}

int DefaultView::totalWidth() const {
  return BoxRenderer(m_columns).totalWidth();
}

std::string DefaultView::banner(const std::vector<StaticInfo>& gpus,
                               const std::string& adlxVersion) const {
  BoxRenderer box(m_columns);

  std::string stamp = currentTimestamp();

  // Driver versions come from the first GPU that reports them; they belong to
  // the driver package rather than to a device. ROCm is a Linux runtime, so the
  // Windows backend reports N/A rather than leaving a misleading blank.
  std::string driver = "N/A";
  std::string winDriver;
  for (const StaticInfo& info : gpus) {
    if (driver == "N/A" && info.driverVersionKnown) {
      driver = info.driverVersion;
    }
    if (winDriver.empty() && info.windowsDriverVersionKnown) {
      winDriver = info.windowsDriverVersion;
    }
  }

  std::string line = "amd-smi-win " + std::string(kAppVersion) + "  |  ADLX " +
                     (adlxVersion.empty() ? std::string("N/A") : adlxVersion) +
                     "  |  Driver " + driver;
  if (!winDriver.empty()) {
    line += " (Adrenalin " + winDriver + ")";
  }
  line += "  |  ROCm N/A (Windows)";
  line += "  |  GPUs " + std::to_string(gpus.size());

  if (!stamp.empty()) {
    line = stamp + "  |  " + line;
  }
  return box.banner(line);
}

std::string DefaultView::staticHeader() const {
  return BoxRenderer(m_columns).headerRow(true);
}

std::string DefaultView::metricHeader() const {
  return BoxRenderer(m_columns).headerRow(false);
}

std::string DefaultView::staticRow(int index, const StaticInfo& info) const {
  std::map<std::string, std::string> values;
  values["gpu"] = std::to_string(index);
  values["name"] = info.name;
  values["device_id"] = info.pciBusIdKnown ? info.pciBusId : info.deviceId;
  return BoxRenderer(m_columns).renderRow(values);
}

std::string DefaultView::metricRow(const Sample& sample,
                                  const StaticInfo& info) const {
  std::map<std::string, std::string> values;

  // GPU index and name live in the static row above, so those cells are left
  // blank here rather than repeating the same values twice.
  if (sample.sclkOk && sample.mclkOk) {
    values["clocks"] = std::to_string(sample.sclkMhz) + "MHz / " +
                       std::to_string(sample.mclkMhz) + "MHz";
  } else if (sample.sclkOk) {
    values["clocks"] = std::to_string(sample.sclkMhz) + "MHz / N/A";
  } else if (sample.mclkOk) {
    values["clocks"] = "N/A / " + std::to_string(sample.mclkMhz) + "MHz";
  } else {
    values["clocks"] = "N/A / N/A";
  }

  values["fan"] = sample.fanOk ? std::to_string(sample.fanRpm) + "RPM" : "N/A";

  std::string usage = sample.powerOk ? formatDouble(sample.power) + "W" : "N/A";
  std::string cap =
      info.powerCapKnown ? std::to_string(info.powerCapWatts) + "W" : "N/A";
  values["power"] = usage + " / " + cap;

  if (sample.vramOk && info.totalVramKnown) {
    values["memory"] = std::to_string(sample.vramUsedMiB) + "MiB / " +
                       std::to_string(info.totalVramMiB) + "MiB";
  } else if (sample.vramOk) {
    values["memory"] = std::to_string(sample.vramUsedMiB) + "MiB / N/A";
  } else if (info.totalVramKnown) {
    values["memory"] = "N/A / " + std::to_string(info.totalVramMiB) + "MiB";
  } else {
    values["memory"] = "N/A / N/A";
  }

  values["util"] =
      sample.usageOk ? formatDouble(sample.usage) + "%" : std::string("N/A");
  values["temp"] = sample.tempOk ? std::to_string((int)sample.temp) + "C" : "N/A";

  return BoxRenderer(m_columns).renderRow(values);
}

std::string DefaultView::rule(char fill) const {
  return BoxRenderer(m_columns).rule(fill);
}

std::string DefaultView::topRule() const {
  // A solid span with a '+' at both corners only, rather than a junction at
  // every column. The offset of the final character is width - 1.
  const int width = totalWidth();
  return BoxRenderer(m_columns).rule('-', {0, width - 1});
}

std::string DefaultView::bottomRule() const {
  std::vector<int> junctions;
  junctions.reserve(m_columns.size() + 1);
  int offset = 0;
  junctions.push_back(offset);
  for (const Column& column : m_columns) {
    offset += column.totalWidth();
    junctions.push_back(offset);
  }
  return BoxRenderer(m_columns).rule('-', junctions);
}
