#include "layout.h"

#include <algorithm>
#include <utility>

namespace {

struct ColumnSpec {
  const char* key;
  const char* top;
  const char* bottom;
  int width;
  int minWidth;
  bool rightAlign;
  int dropPriority;
  bool groupStart;
};

// nvidia-smi's default view is a three-block table:
//
//   | GPU   Name   Persistence-M | Bus-Id        Disp.A | Volatile Uncorr. ECC |
//   | Fan  Temp  Perf  Pwr:Usage/Cap|      Memory-Usage | GPU-Util  Compute M. |
//   |====================================================+======================|
//
// The blocks are the identity of the device, its live telemetry, and the
// per-device state. Columns inside a block are space separated; only the block
// boundaries are ruled. We keep the same shape and the same two header rows:
// `top` labels the identity row, `bottom` the telemetry row.
//
// The first block is the "static" half of the nvidia-smi header, the second is
// the "metric" half. Labels are centred by BoxRenderer, so the two halves of
// the header line up the way they do in nvidia-smi.
//
// Widths are nvidia-smi-tight: the whole set fits a 110-column console, so a
// normally sized window shows every statistic.
//
// `minWidth` is the point past which a column is squeezed as far as it usefully
// goes. Only `name` has slack, and that mirrors nvidia-smi, which also treats
// the device name as the elastic field and truncates it with '~'. The numeric
// columns are rigid at the width of their widest realistic value: shortening
// one would truncate the reading itself (0x...), which is far worse than
// dropping a whole column that the user can get back by widening the window.
constexpr ColumnSpec kSpecs[] = {
    // Identity block.
    {"gpu", "GPU", "", 3, 3, false, kEssential, true},
    // 21 fits the longest shipping consumer name ("NVIDIA GeForce RTX 4090")
    // with room to spare, and it is the column nvidia-smi also gives its name.
    {"name", "Name", "", 21, 15, false, kEssential, false},
    // Telemetry block.
    {"temp", "", "Temp", 4, 4, true, kEssential, true},
    {"fan", "", "Fan", 7, 7, true, 200, false},
    {"power", "", "Pwr:Usage/Cap", 13, 13, true, 100, false},
    {"memory", "", "Memory-Usage", 15, 15, true, kEssential, false},
    {"util", "", "GPU-Util", 8, 8, true, kEssential, false},
    {"clocks", "", "SCLK/MCLK", 13, 13, true, 300, false},
    // Per-device state block.
    {"device_id", "PCI-ID", "", 8, 8, false, 400, true},
};

}  // namespace

std::vector<Column> defaultColumns() {
  std::vector<Column> columns;
  columns.reserve(std::size(kSpecs));
  for (const ColumnSpec& spec : kSpecs) {
    columns.push_back(Column{std::string(spec.key), std::string(spec.top),
                             std::string(spec.bottom), spec.width,
                             spec.rightAlign, spec.minWidth, spec.groupStart});
  }
  return columns;
}

// Squeezes the flexible columns toward their minimum widths without dropping
// anything. The widest flexible column gives up a character at a time, which
// spreads the truncation evenly instead of mangling one column while another
// keeps slack.
static std::vector<Column> compressColumns(const std::vector<Column>& columns) {
  std::vector<Column> result(columns);
  for (;;) {
    auto widest = std::max_element(
        result.begin(), result.end(), [](const Column& a, const Column& b) {
          if (a.shrinkable() != b.shrinkable()) {
            return !a.shrinkable();
          }
          return a.width < b.width;
        });
    if (widest == result.end() || !widest->shrinkable()) {
      return result;
    }
    --widest->width;
  }
}

int compressedWidth(const std::vector<Column>& columns) {
  return tableWidth(compressColumns(columns));
}

std::vector<Column> fitColumns(const std::vector<Column>& columns,
                               int terminalWidth) {
  if (terminalWidth <= 0 || tableWidth(columns) <= terminalWidth) {
    return columns;
  }

  // Phase 1: squeeze the flexible columns before dropping anything, so a tight
  // console shows every statistic in a narrower form instead of losing whole
  // columns.
  std::vector<Column> result = compressColumns(columns);
  if (tableWidth(result) <= terminalWidth) {
    return result;
  }

  // Phase 2: compression is exhausted, so drop the least important columns.
  // Build the drop order: (dropPriority, key), sorted so the column most safe
  // to lose (highest dropPriority) is removed first. kEssential columns never
  // enter the list.
  std::vector<std::pair<int, std::string>> droppable;
  for (const Column& column : result) {
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
