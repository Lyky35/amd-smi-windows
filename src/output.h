#pragma once

#include <string>
#include <vector>

#include "cli.h"

// A row-major result table. The first row holds column headers.
struct Table {
  std::vector<std::string> headers;
  std::vector<std::vector<std::string>> rows;

  void add(std::vector<std::string> row) { rows.push_back(std::move(row)); }
};

// Writes `table` in the requested format to stdout.
void render(const Table& table, OutputFormat format);

// A JSON document is modelled as a flat object whose values are either
// scalars or nested objects. Enough for this CLI, and avoids vendoring a
// JSON library into a cross-compiled target.
class JsonBuilder {
 public:
  explicit JsonBuilder(OutputFormat format);

  // Starts a new object under `key` (or the document root when key is empty).
  void beginObject(const std::string& key);
  void endObject();

  // Note: the `const char*` overload is not redundant. Without it, a string
  // literal resolves to the `bool` overload, because `const char*` -> `bool` is
  // a standard conversion and outranks the user-defined conversion to
  // `std::string`.
  void field(const std::string& key, const char* value);
  void field(const std::string& key, const std::string& value);
  void field(const std::string& key, long long value);
  void field(const std::string& key, double value);
  void field(const std::string& key, bool value);

  // Emits a value only when `ok`; writes null otherwise.
  void optionalField(const std::string& key, bool ok, const std::string& value);
  void optionalField(const std::string& key, bool ok, double value);

  void finish();

 private:
  OutputFormat m_format;
  int m_depth = 0;
  std::vector<bool> m_first;
  std::string m_buffer;

  // Emits the separator/newline preceding the next value at the current depth.
  void itemPrefix();
  void indent();
};

std::string jsonEscape(const std::string& in);

// Formats a double with up to `decimals` places, trimming trailing zeros.
// Returns "N/A" for non-finite input so metrics never print as "nan".
std::string formatDouble(double value, int decimals = 1);
std::string formatBytes(long long bytes);

// Application version, injected by CMake from the project version so that the
// banner, --version and README cannot drift apart.
#ifndef AMD_SMI_VERSION
#define AMD_SMI_VERSION "0.0.0"
#endif
inline constexpr const char* kAppVersion = AMD_SMI_VERSION;
