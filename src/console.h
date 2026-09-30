#pragma once

// Number of columns available for output, or 0 when it cannot be determined
// (output redirected to a file or pipe, or no console attached).
int detectConsoleWidth();
