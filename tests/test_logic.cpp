// Host-side tests for the platform-independent logic (argument parsing,
// formatting, and the renderers). These compile and run natively on Linux so
// the Windows-only ADLX code paths can be exercised without a Windows host.

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <unistd.h>

#include "box.h"
#include "cli.h"
#include "default_view.h"
#include "layout.h"
#include "output.h"

static int g_failures = 0;
static int g_checks = 0;

static int countChar(const std::string& s, char c) {
  int n = 0;
  for (char ch : s) {
    if (ch == c) {
      ++n;
    }
  }
  return n;
}

static void check(bool condition, const char* what) {
  ++g_checks;
  if (!condition) {
    ++g_failures;
    std::printf("  FAIL: %s\n", what);
  }
}

static void checkEq(const std::string& actual, const std::string& expected,
                    const char* what) {
  ++g_checks;
  if (actual != expected) {
    ++g_failures;
    std::printf("  FAIL: %s\n        expected: '%s'\n        actual:   '%s'\n",
                what, expected.c_str(), actual.c_str());
  }
}

// argv needs to be mutable for parseArgs.
static int parse(std::vector<std::string> args, std::string& command,
                 Options& opts) {
  std::vector<char*> argv;
  argv.push_back(const_cast<char*>("amd-smi"));
  for (auto& a : args) {
    argv.push_back(const_cast<char*>(a.c_str()));
  }
  return parseArgs(static_cast<int>(argv.size()), argv.data(), command, opts);
}

static void testFormatDouble() {
  std::printf("formatDouble\n");
  checkEq(formatDouble(12.345), "12.3", "trims to 1 decimal by default");
  checkEq(formatDouble(12.0), "12", "drops trailing .0");
  checkEq(formatDouble(0.0), "0", "zero");
  checkEq(formatDouble(1234.5678, 2), "1234.57", "honours explicit decimals");
  checkEq(formatDouble(std::nan("")), "N/A", "NaN renders as N/A");
  checkEq(formatDouble(INFINITY), "N/A", "infinity renders as N/A");
}

static void testFormatBytes() {
  std::printf("formatBytes\n");
  checkEq(formatBytes(1024LL), "1.00 KiB", "KiB");
  checkEq(formatBytes(1024LL * 1024 * 8), "8.00 MiB", "MiB");
  checkEq(formatBytes(1024LL * 1024 * 1024 * 16), "16.00 GiB", "GiB");
  checkEq(formatBytes(-1), "N/A", "negative renders as N/A");
}

static void testJsonEscape() {
  std::printf("jsonEscape\n");
  checkEq(jsonEscape("plain"), "plain", "plain string");
  checkEq(jsonEscape("say \"hi\""), "say \\\"hi\\\"", "quotes");
  checkEq(jsonEscape("a\\b"), "a\\\\b", "backslash");
  checkEq(jsonEscape("line\nbreak"), "line\\nbreak", "newline");
  checkEq(jsonEscape(std::string("ctrl\x01")), "ctrl\\u0001", "control char");
}

static void testGpuSelector() {
  std::printf("gpu selector\n");
  std::string cmd;
  Options opts;

  opts = Options();
  parse({"static", "--gpu", "0"}, cmd, opts);
  check(cmd == "static", "command captured");
  check(opts.gpus.size() == 1 && opts.gpus[0] == 0, "single index");

  opts = Options();
  parse({"metric", "--gpu", "0,1,3"}, cmd, opts);
  check(opts.gpus.size() == 3 && opts.gpus[0] == 0 && opts.gpus[1] == 1 &&
            opts.gpus[2] == 3,
        "comma list");

  opts = Options();
  parse({"metric", "--gpu", "0-3"}, cmd, opts);
  check(opts.gpus.size() == 4 && opts.gpus[3] == 3, "range expands");

  opts = Options();
  parse({"metric", "--gpu", "all"}, cmd, opts);
  check(opts.gpus.empty(), "'all' clears the selection");

  opts = Options();
  parse({"metric", "--gpu", "0,2-3"}, cmd, opts);
  check(opts.gpus.size() == 3 && opts.gpus[1] == 2 && opts.gpus[2] == 3,
        "mixed list and range");
}

static void testFlags() {
  std::printf("flags\n");
  std::string cmd;
  Options opts;

  opts = Options();
  parse({"--json", "metric"}, cmd, opts);
  check(cmd == "metric", "global flag before command");
  check(opts.format == OutputFormat::Json, "--json before command");

  opts = Options();
  parse({"metric", "--json"}, cmd, opts);
  check(opts.format == OutputFormat::Json, "--json after command");

  opts = Options();
  parse({"metric", "--csv"}, cmd, opts);
  check(opts.format == OutputFormat::Csv, "--csv");

  opts = Options();
  parse({"metric", "-i", "500", "-n", "10"}, cmd, opts);
  check(opts.intervalMs == 500, "interval parsed");
  check(opts.iterations == 10, "iterations parsed");

  opts = Options();
  check(parse({"metric", "-i"}, cmd, opts) != 0, "missing interval is an error");
  check(parse({"metric", "-n", "0"}, cmd, opts) != 0, "zero iterations rejected");
  check(parse({"metric", "--bogus"}, cmd, opts) != 0, "unknown option rejected");
  check(parse({"static", "extra"}, cmd, opts) != 0, "stray positional rejected");

  opts = Options();
  parse({"-h"}, cmd, opts);
  check(opts.help, "-h sets help");
}

// Captures stdout while `fn` runs, and returns what was written.
template <typename Fn>
static std::string captureStdout(Fn fn) {
  std::fflush(stdout);
  int saved = dup(fileno(stdout));
  FILE* tmp = tmpfile();
  dup2(fileno(tmp), fileno(stdout));

  fn();

  std::fflush(stdout);
  dup2(saved, fileno(stdout));
  close(saved);

  std::rewind(tmp);
  std::string out;
  char buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof(buf), tmp)) > 0) {
    out.append(buf, n);
  }
  std::fclose(tmp);
  return out;
}

static bool contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

static void testJsonBuilder() {
  std::printf("JsonBuilder\n");

  std::string out = captureStdout([] {
    JsonBuilder json(OutputFormat::Json);
    json.beginObject("");
    json.field("name", "Radeon RX 9070");
    json.field("external", true);
    json.field("vram_total_bytes", 16LL * 1024 * 1024 * 1024);
    json.beginObject("gpus");
    json.beginObject("0");
    json.field("type", "DISCRETE");
    json.optionalField("temperature_c", false, 0.0);
    json.optionalField("usage_percent", true, 42.5);
    json.field("escaped", "quote\" and \\slash");
    json.endObject();
    json.endObject();
    json.endObject();
    json.finish();
  });

  std::printf("%s", out.c_str());

  check(contains(out, "\"name\": \"Radeon RX 9070\""),
        "string literal is quoted, not coerced to bool");
  check(contains(out, "\"external\": true"), "bool stays a bool");
  check(contains(out, "\"vram_total_bytes\": 17179869184"), "integer not quoted");
  check(contains(out, "\"usage_percent\": 42.5000"), "double is numeric");
  check(contains(out, "\"temperature_c\": null"), "unsupported metric is null");
  check(contains(out, "\"escaped\": \"quote\\\" and \\\\slash\""),
        "quotes and backslashes escaped");
  check(!contains(out, "\"type\": true"), "nested string literal not a bool");

  // Every field must sit on its own line; a missing newline collapses the
  // document into an unreadable single line.
  size_t lines = 0;
  for (char c : out) {
    if (c == '\n') {
      ++lines;
    }
  }
  check(lines >= 9, "output is one field per line");

  check(out.front() == '{' && out.find('}') != std::string::npos,
        "document is a single braced object");
  check(countChar(out, '{') == countChar(out, '}'), "braces are balanced");
}

static void testTableRendering() {
  std::printf("Table/CSV rendering\n");
  Table table;
  table.headers = {"GPU", "NAME", "UTIL%"};
  table.add({"0", "Radeon RX 9070", "12%"});
  table.add({"1", "Radeon Graphics", "N/A"});

  std::string out = captureStdout([&] { render(table, OutputFormat::Table); });
  std::printf("%s", out.c_str());

  check(contains(out, "|GPU|NAME           |UTIL%|"), "table aligns columns");
  check(contains(out, "|1  |Radeon Graphics|N/A  |"), "rows padded to width");

  std::string csv = captureStdout([&] { render(table, OutputFormat::Csv); });
  std::printf("%s", csv.c_str());
  check(contains(csv, "GPU,NAME,UTIL%\n"), "csv header row");
  check(contains(csv, "0,Radeon RX 9070,12%\n"), "csv data row");
}

static void testCsvEscaping() {
  std::printf("CSV escaping\n");
  Table table;
  table.headers = {"A", "B"};
  table.add({"has,comma", "has\"quote"});

  std::string csv = captureStdout([&] { render(table, OutputFormat::Csv); });
  std::printf("%s", csv.c_str());
  check(contains(csv, "\"has,comma\",\"has\"\"quote\""),
        "csv quotes separators and doubles quotes");
}

static void testFit() {
  std::printf("BoxRenderer::fit\n");
  checkEq(BoxRenderer::fit("ab", 5, false), "ab   ", "left pad");
  checkEq(BoxRenderer::fit("ab", 5, true), "   ab", "right pad");
  checkEq(BoxRenderer::fit("exactly10!", 10, false), "exactly10!", "exact fit");
  checkEq(BoxRenderer::fit("abcdefghij", 5, false), "abcd~", "truncation marker");
  checkEq(BoxRenderer::fit("", 3, true), "   ", "empty pads");
  checkEq(BoxRenderer::fit("x", 0, false), "", "zero width is safe");
}

static int rowsTotalWidth(const std::vector<Column>& columns) {
  return BoxRenderer(columns).totalWidth();
}

static bool hasKey(const std::vector<Column>& columns, const std::string& key) {
  for (const Column& column : columns) {
    if (column.key == key) {
      return true;
    }
  }
  return false;
}

static void testFitColumns() {
  std::printf("fitColumns\n");
  const int full = rowsTotalWidth(defaultColumns());
  check(full == 144, "full dashboard is 144 chars wide");

  // Unknown width means "redirected": keep everything so captured output is not
  // quietly truncated.
  check(fitColumns(defaultColumns(), 0).size() == defaultColumns().size(),
        "unknown width keeps all columns");

  // Generous terminals keep every column.
  check(fitColumns(defaultColumns(), full).size() == defaultColumns().size(),
        "exact fit keeps all");
  check(fitColumns(defaultColumns(), full + 40).size() == defaultColumns().size(),
        "wide terminal keeps all");

  // Dropping happens in priority order: Device ID first, then SCLK/MCLK,
  // Fan, power. The essentials (GPU, Name, Temp, Memory, Util) never go.
  std::vector<Column> narrow1 = fitColumns(defaultColumns(), full - 1);
  check(narrow1.size() == defaultColumns().size() - 1,
        "one droppable column goes first");
  check(!hasKey(narrow1, "device_id"), "Device ID dropped first");

  std::vector<Column> narrow2 = fitColumns(defaultColumns(), full - 1 -
                                                             16);
  check(narrow2.size() == defaultColumns().size() - 2,
        "second column dropped as width shrinks");
  check(!hasKey(narrow2, "clocks"), "SCLK/MCLK dropped second");

  for (const char* key : {"gpu", "name", "temp", "memory", "util"}) {
    std::vector<Column> tiny = fitColumns(defaultColumns(), 20);
    check(hasKey(tiny, key), "essential column never dropped");
  }

  // The trimmed table must actually fit the target width.
  for (int w : {80, 100, 110}) {
    std::vector<Column> cols = fitColumns(defaultColumns(), w);
    check(rowsTotalWidth(cols) <= w, "trimmed table fits the requested width");
  }
}

static void testBoxAlignment() {
  std::printf("BoxRenderer alignment\n");
  std::vector<Column> columns = defaultColumns();
  BoxRenderer box(columns);
  const int width = box.totalWidth();

  checkEq(std::to_string(width), "144", "declared table width");
  check(!hasKey(columns, "type"), "default view has no Type column");

  // Offset of the '|' that closes each column, derived from the column list
  // rather than hardcoded, so the assertion cannot drift from the layout.
  std::vector<int> junctions;
  int offset = 0;
  for (const Column& column : columns) {
    offset += column.totalWidth();
    junctions.push_back(offset);
  }
  check(junctions.back() == width - 1, "last junction is the final character");

  // Every emitted line must be exactly tableWidth characters, or the borders
  // will not line up in a terminal.
  std::vector<std::string> lines = {
      box.rule('-'),
      box.rule('='),
      box.rule('-', junctions),
      box.headerRow(true),
      box.headerRow(false),
      box.banner(" amd-smi-win 0.1.0 | ADLX Version: 1.5.0.0"),
      box.renderRow({"0", "Radeon RX 9070", "0x744C"}),
      box.renderRow({"1"}),
      box.renderRow(std::vector<std::string>(9, "")),
      box.renderRow({"2", "An Extremely Long GPU Name That Will Not Fit At All",
                     "0x164E", "45C", "N/A", "N/A / 15W",
                     "512MiB / 4096MiB", "3%", "800MHz / 9000MHz"}),
  };

  for (size_t i = 0; i < lines.size(); ++i) {
    checkEq(std::to_string(lines[i].size()), std::to_string(width),
            "line width matches table width");
    check(lines[i].front() == '+' || lines[i].front() == '|',
          "line opens with a border character");
  }

  // A row shorter than the column count must still render full width.
  checkEq(std::to_string(lines[7].size()), std::to_string(width),
          "short row pads to full width");

  checkEq(box.rule('-'), box.rule('-'), "rules are deterministic");
  check(box.rule('=') != box.rule('-'), "fill character is honoured");

  // Every column boundary in a plain rule is a '+'.
  std::string plain = box.rule('-');
  for (size_t i = 0; i < junctions.size(); ++i) {
    check(plain[junctions[i]] == '+', "plain rule has '+' at every boundary");
  }

  // Junction junctions are '+', and non-junction offsets keep the fill.
  std::string junctionRule = box.rule('-', junctions);
  for (size_t i = 0; i < junctions.size(); ++i) {
    check(junctionRule[junctions[i]] == '+', "junction renders as '+'");
  }
  check(junctionRule[0] == '+', "leading junction");

  check(box.headerRow(true) != box.headerRow(false),
        "top and bottom headers differ");

  // Rows are resolved by column key rather than position, so a superset map
  // renders only the columns that survived the width selection.
  std::string wide = captureStdout([] {
    DefaultView view(200);
    std::printf("%s\n", view.staticRow(0, StaticInfo()).c_str());
    std::printf("%s\n", view.metricRow(Sample(), StaticInfo()).c_str());
  });
  check(contains(wide, "| 0    |"), "GPU index rendered");
  check(!contains(wide, "Type"), "no Type column rendered");
  check(contains(wide, "N/A / N/A"), "missing power renders both halves N/A");

  std::string out = captureStdout([] {
    BoxRenderer box2(defaultColumns());
    std::printf("%s\n", box2.rule('=').c_str());
    std::printf("%s\n", box2.headerRow(true).c_str());
    std::printf("%s\n", box2.headerRow(false).c_str());
    std::printf("%s\n", box2.renderRow({"0", "Radeon RX 9070", "0x744C"})
                            .c_str());
    std::printf("%s\n",
                box2.renderRow({"", "", "", "41C", "2340RPM", "15W / 450W",
                                "1024MiB / 16384MiB", "0%",
                                "1200MHz / 14000MHz"})
                    .c_str());
    std::printf("%s\n", box2.rule('-').c_str());
  });
  std::printf("%s", out.c_str());
}

static void testDefaultView() {
  std::printf("DefaultView\n");

  StaticInfo info;
  info.name = "Radeon RX 9070";
  info.deviceId = "0x744C";
  info.pciBusIdKnown = true;
  info.pciBusId = "01:00.0";
  info.totalVramKnown = true;
  info.totalVramMiB = 16384;
  info.powerCapKnown = true;
  info.powerCapWatts = 450;

  Sample sample;
  sample.usageOk = sample.tempOk = sample.powerOk = sample.vramOk = true;
  sample.powerOk = true;
  sample.vramOk = true;
  sample.usage = 12.0;
  sample.temp = 41.0;
  sample.power = 15.0;
  sample.vramUsedMiB = 1024;

  // A narrow console must drop columns but keep the memory totals readable.
  DefaultView narrow(78);
  std::string row = narrow.metricRow(sample, info);
  check(row.size() <= 78, "narrow row fits the window");
  check(contains(row, "1024MiB / 16384MiB"), "total VRAM from TotalVRAM (MiB)");
  check(!contains(row, "RPM"), "fan column dropped at 78 columns");
  check(!contains(row, "MHz"), "clock column dropped at 78 columns");

  DefaultView wide(200);
  std::string full = wide.metricRow(sample, info);
  check(contains(full, "15W / 450W"), "usage cap and limit rendered");
  check(contains(full, "1024MiB / 16384MiB"), "memory shown at full width");
  check(!contains(full, "Radeon RX 9070") && !contains(full, "| 0    |"),
        "metric row leaves GPU index and name to the static row");

  // Missing info degrades to N/A without crashing or emitting garbage.
  std::string blank = wide.metricRow(Sample(), StaticInfo());
  check(contains(blank, "N/A"), "blank sample renders as N/A");

  // The static row prefers the PCI bus id (nvidia-smi's Bus-Id identity) and
  // falls back to the chip device id when the BDF lookup failed.
  DefaultView medium(160);
  std::string staticLine = medium.staticRow(0, info);
  check(contains(staticLine, "01:00.0 "), "PCI bus id shown in the static row");
  StaticInfo noBdf = info;
  noBdf.pciBusIdKnown = false;
  std::string fallback = medium.staticRow(0, noBdf);
  check(contains(fallback, "0x744C"), "chip device id fallback when no BDF");
}

int main() {
  std::printf("amd-smi-win host tests\n\n");
  testFormatDouble();
  testFormatBytes();
  testJsonEscape();
  testGpuSelector();
  testFlags();
  testJsonBuilder();
  testTableRendering();
  testCsvEscaping();
  testFit();
  testFitColumns();
  testBoxAlignment();
  testDefaultView();

  std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
