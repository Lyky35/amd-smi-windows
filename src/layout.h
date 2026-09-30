#pragma once

#include <vector>

#include "box.h"

// Column priorities for the default dashboard, from most to least expendable.
// kEssential columns are never dropped, even if that means overflowing a very
// narrow terminal: a slightly wide table beats a table missing the GPU index.
constexpr int kEssential = 1 << 30;

// The full dashboard column set, in display order.
std::vector<Column> defaultColumns();

// Drops the least important columns until the table fits in `terminalWidth`.
// Pass 0 when the width is unknown (output redirected to a file or pipe) to get
// the full, untruncated set.
std::vector<Column> fitColumns(const std::vector<Column>& columns,
                               int terminalWidth);
