#include "output.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Formatting helpers
// ---------------------------------------------------------------------------

std::string formatDouble(double value, int decimals) {
  if (!std::isfinite(value)) {
    return "N/A";
  }
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.*f", decimals, value);

  // Trim trailing zeros (and a dangling decimal point) for a tighter table.
  std::string s(buf);
  if (s.find('.') != std::string::npos) {
    while (!s.empty() && s.back() == '0') {
      s.pop_back();
    }
    if (!s.empty() && s.back() == '.') {
      s.pop_back();
    }
  }
  return s.empty() ? std::string("0") : s;
}

std::string formatBytes(long long bytes) {
  if (bytes < 0) {
    return "N/A";
  }
  const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
  double value = static_cast<double>(bytes);
  int unit = 0;
  while (value >= 1024.0 && unit < 4) {
    value /= 1024.0;
    ++unit;
  }
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.2f %s", value, units[unit]);
  return buf;
}

std::string jsonEscape(const std::string& in) {
  std::string out;
  out.reserve(in.size() + 8);
  for (unsigned char c : in) {
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b";  break;
      case '\f': out += "\\f";  break;
      case '\n': out += "\\n";  break;
      case '\r': out += "\\r";  break;
      case '\t': out += "\\t";  break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  return out;
}

// ---------------------------------------------------------------------------
// Table / CSV
// ---------------------------------------------------------------------------

namespace {

std::string escapeCsv(const std::string& in) {
  bool needsQuotes = in.find_first_of(",\"\n\r") != std::string::npos;
  if (!needsQuotes) {
    return in;
  }
  std::string out = "\"";
  for (char c : in) {
    if (c == '"') {
      out += "\"\"";
    } else {
      out += c;
    }
  }
  out += "\"";
  return out;
}

void renderCsv(const Table& table) {
  auto emit = [](const std::vector<std::string>& cells) {
    std::string line;
    for (size_t i = 0; i < cells.size(); ++i) {
      if (i != 0) {
        line += ",";
      }
      line += escapeCsv(cells[i]);
    }
    std::printf("%s\n", line.c_str());
  };

  emit(table.headers);
  for (const auto& row : table.rows) {
    emit(row);
  }
}

void renderTable(const Table& table) {
  if (table.headers.empty()) {
    return;
  }

  std::vector<size_t> widths(table.headers.size(), 0);
  auto measure = [&widths](const std::vector<std::string>& cells) {
    for (size_t i = 0; i < cells.size() && i < widths.size(); ++i) {
      widths[i] = std::max(widths[i], cells[i].size());
    }
  };

  measure(table.headers);
  for (const auto& row : table.rows) {
    measure(row);
  }

  std::string separator;
  for (size_t i = 0; i < widths.size(); ++i) {
    separator += (i == 0) ? "" : "+";
    separator.append(widths[i], '-');
  }

  auto emit = [&widths](const std::vector<std::string>& cells) {
    std::string line;
    for (size_t i = 0; i < widths.size(); ++i) {
      line += "|";
      const std::string& cell = (i < cells.size()) ? cells[i] : std::string();
      line += cell;
      line.append(widths[i] - cell.size(), ' ');
    }
    line += "|";
    std::printf("%s\n", line.c_str());
  };

  emit(table.headers);
  std::printf("|%s|\n", separator.c_str());
  for (const auto& row : table.rows) {
    emit(row);
  }
}

}  // namespace

void render(const Table& table, OutputFormat format) {
  switch (format) {
    case OutputFormat::Json: {
      // Tables carry heterogeneous column types, so JSON emission is handled
      // by the commands through JsonBuilder. Falling back to CSV-shaped JSON
      // would lose typing; emit nothing rather than something misleading.
      std::fprintf(stderr,
                   "error: this command does not support --json output\n");
      break;
    }
    case OutputFormat::Csv:
      renderCsv(table);
      break;
    case OutputFormat::Table:
    default:
      renderTable(table);
      break;
  }
}

// ---------------------------------------------------------------------------
// JsonBuilder
// ---------------------------------------------------------------------------

JsonBuilder::JsonBuilder(OutputFormat format) : m_format(format) {}

void JsonBuilder::itemPrefix() {
  if (m_first.empty()) {
    return;
  }
  if (m_first.back()) {
    m_first.back() = false;  // beginObject already emitted the newline
  } else {
    m_buffer += ",\n";
  }
}

void JsonBuilder::indent() {
  m_buffer.append(static_cast<size_t>(m_depth) * 2, ' ');
}

void JsonBuilder::beginObject(const std::string& key) {
  itemPrefix();
  indent();
  if (!key.empty()) {
    m_buffer += "\"" + jsonEscape(key) + "\": ";
  }
  m_buffer += "{\n";
  ++m_depth;
  m_first.push_back(true);
}

void JsonBuilder::endObject() {
  m_buffer += "\n";
  --m_depth;
  m_first.pop_back();
  indent();
  m_buffer += "}";
}

void JsonBuilder::field(const std::string& key, const char* value) {
  itemPrefix();
  indent();
  m_buffer += "\"" + jsonEscape(key) + "\": \"" +
              jsonEscape(value != nullptr ? value : "") + "\"";
}

void JsonBuilder::field(const std::string& key, const std::string& value) {
  field(key, value.c_str());
}

void JsonBuilder::field(const std::string& key, long long value) {
  itemPrefix();
  indent();
  m_buffer += "\"" + jsonEscape(key) + "\": " + std::to_string(value);
}

void JsonBuilder::field(const std::string& key, double value) {
  itemPrefix();
  indent();
  if (std::isfinite(value)) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.4f", value);
    m_buffer += "\"" + jsonEscape(key) + "\": " + buf;
  } else {
    m_buffer += "\"" + jsonEscape(key) + "\": null";
  }
}

void JsonBuilder::field(const std::string& key, bool value) {
  itemPrefix();
  indent();
  m_buffer += "\"" + jsonEscape(key) + "\": " + (value ? "true" : "false");
}

void JsonBuilder::optionalField(const std::string& key, bool ok,
                                const std::string& value) {
  if (ok) {
    field(key, value);
  } else {
    itemPrefix();
    indent();
    m_buffer += "\"" + jsonEscape(key) + "\": null";
  }
}

void JsonBuilder::optionalField(const std::string& key, bool ok, double value) {
  if (ok && std::isfinite(value)) {
    field(key, value);
  } else {
    itemPrefix();
    indent();
    m_buffer += "\"" + jsonEscape(key) + "\": null";
  }
}

void JsonBuilder::finish() {
  while (m_depth > 0) {
    endObject();
  }
  if (m_format == OutputFormat::Json) {
    std::printf("%s\n", m_buffer.c_str());
  }
}
