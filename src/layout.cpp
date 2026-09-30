#include "layout.h"

#include <algorithm>
#include <utility>

namespace {

struct ColumnSpec {
  const char* key;
  const char* top;
  const char* bottom;
  int width;
  bool rightAlign;
  int dropPriority;
};

// The first block is the "static" half of the header, the second is the
// "metric" half. Labels are centred by BoxRenderer, so the two halves of the
// header line up.
constexpr ColumnSpec kSpecs[] = {
    {"gpu", "GPU", "", 4, false, kEssential},
    {"name", "Name", "", 22, false, kEssential},
    {"device_id", "PCI-ID", "", 10, false, 400},
    {"temp", "", "Temp", 6, true, kEssential},
    {"fan", "", "Fan", 9, true, 200},
    {"power", "", "Pwr:Usage/Cap", 18, true, 100},
    {"memory", "", "Memory-Usage", 20, true, kEssential},
    {"util", "", "GPU-Util", 9, true, kEssential},
    {"clocks", "", "SCLK / MCLK", 18, true, 300},
};

int tableWidth(const std::vector<Column>& columns) {
  // Matches BoxRenderer::totalWidth(): the leading border plus, per column,
  // "| " + content + " |" (totalWidth() includes the trailing separator).
  int total = 1;
  for (const Column& column : columns) {
    total += column.totalWidth();
  }
  return total;
}

}  // namespace

std::vector<Column> defaultColumns() {
  std::vector<Column> columns;
  columns.reserve(std::size(kSpecs));
  for (const ColumnSpec& spec : kSpecs) {
    columns.push_back(Column{std::string(spec.key), std::string(spec.top),
                             std::string(spec.bottom), spec.width,
                             spec.rightAlign});
  }
  return columns;
}

std::vector<Column> fitColumns(const std::vector<Column>& columns,
                               int terminalWidth) {
  if (terminalWidth <= 0 || tableWidth(columns) <= terminalWidth) {
    return columns;
  }

  // Build the drop order: (dropPriority, key), sorted so the column most safe
  // to lose (highest dropPriority) is removed first. kEssential columns never
  // enter the list.
  std::vector<std::pair<int, std::string>> droppable;
  for (const Column& column : columns) {
    for (const ColumnSpec& spec : kSpecs) {
      if (column.key == spec.key && spec.dropPriority < kEssential) {
        droppable.emplace_back(spec.dropPriority, column.key);
        break;
      }
    }
  }
  std::sort(droppable.begin(), droppable.end(),
            [](const std::pair<int, std::string>& a,
               const std::pair<int, std::string>& b) { return a.first > b.first; });

  std::vector<Column> result(columns);
  for (const std::pair<int, std::string>& drop : droppable) {
    auto it = std::find_if(result.begin(), result.end(),
                           [&](const Column& c) { return c.key == drop.second; });
    if (it != result.end()) {
      result.erase(it);
    }
    if (tableWidth(result) <= terminalWidth) {
      break;
    }
  }
  return result;
}
