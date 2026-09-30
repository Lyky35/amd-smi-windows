#include "console.h"

#include <cstdlib>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace {

// A COLUMNS override wins everywhere. Terminal multiplexers and shells such as
// MSYS2/Cygwin set it, and it lets the user force a width for a narrower
// window or for capture.
int widthFromEnvironment() {
  const char* value = std::getenv("COLUMNS");
  if (value == nullptr || *value == '\0') {
    return 0;
  }
  char* end = nullptr;
  long parsed = std::strtol(value, &end, 10);
  if (*end != '\0' || parsed <= 0 || parsed > 4096) {
    return 0;
  }
  return static_cast<int>(parsed);
}

}  // namespace

int detectConsoleWidth() {
  if (int override = widthFromEnvironment(); override > 0) {
    return override;
  }

#if defined(_WIN32)
  CONSOLE_SCREEN_BUFFER_INFO info;
  HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
  if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  if (!GetConsoleScreenBufferInfo(handle, &info)) {
    // Redirected output: there is no window to measure.
    return 0;
  }
  return info.srWindow.Right - info.srWindow.Left + 1;
#else
  struct winsize size;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) != 0 || size.ws_col == 0) {
    return 0;
  }
  return size.ws_col;
#endif
}
