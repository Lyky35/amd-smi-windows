#pragma once

#include <vector>

#include "box.h"

// Column priorities for the default dashboard, from most to least expendable.
// kEssential columns are never dropped, even if that means overflowing a very
// narrow terminal: a slightly wide table beats a table missing the GPU index.
constexpr int kEssential = 1 << 30;

// The full dashboard column set, in display order.
std::vector<Column> defaultColumns();

// Width of `columns` once every flexible column has been squeezed as far as it
// usefully goes, but nothing has been dropped. This is the narrowest the table
// can be while still showing every statistic, and the width below which
// fitColumns() starts removing columns instead.
int compressedWidth(const std::vector<Column>& columns);

// Drops the least important columns until the table fits in `terminalWidth`.
// Pass 0 when the width is unknown (output redirected to a file or pipe) to get
// the full, untruncated set.
std::vector<Column> fitColumns(const std::vector<Column>& columns,
                               int terminalWidth);
