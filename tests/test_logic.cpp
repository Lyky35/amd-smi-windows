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

static void check(bool condition, const std::string& what) {
  ++g_checks;
  if (!condition) {
    ++g_failures;
    std::printf("  FAIL: %s\n", what.c_str());
  }
}

static void checkEq(const std::string& actual, const std::string& expected,
                    const std::string& what) {
  ++g_checks;
  if (actual != expected) {
    ++g_failures;
    std::printf("  FAIL: %s\n        expected: '%s'\n        actual:   '%s'\n",
                what.c_str(), expected.c_str(), actual.c_str());
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
  check(full == 104, "full dashboard is 104 chars wide, nvidia-smi dense");

  // Unknown width means "redirected": keep everything so captured output is not
  // quietly truncated.
  check(fitColumns(defaultColumns(), 0).size() == defaultColumns().size(),
        "unknown width keeps all columns");

  // Generous terminals keep every column at full width.
  check(fitColumns(defaultColumns(), full).size() == defaultColumns().size(),
        "exact fit keeps all");
  check(fitColumns(defaultColumns(), full + 40).size() == defaultColumns().size(),
        "wide terminal keeps all");

  // Below the full width the GPU name gives up characters first, so every
  // statistic stays visible at full precision and only the (cosmetic) name
  // truncates, exactly like nvidia-smi. The floor is the name's own minimum.
  for (int w : {113, 112}) {
    std::vector<Column> cols = fitColumns(defaultColumns(), w);
    check(cols.size() == defaultColumns().size(),
          "no column dropped while the name can compress (" +
              std::to_string(w) + " cols)");
    check(rowsTotalWidth(cols) <= w,
          "compressed table fits " + std::to_string(w));
  }

  // No reading is ever truncated: the numeric columns are rigid at the width of
  // their widest value, so compression can only ever take characters from the
  // name.
  for (int w : {113, 112}) {
    for (const Column& column : fitColumns(defaultColumns(), w)) {
      if (column.key != "name") {
        Column full;
        for (const Column& candidate : defaultColumns()) {
          if (candidate.key == column.key) {
            full = candidate;
          }
        }
        checkEq(std::to_string(column.width), std::to_string(full.width),
                "column " + column.key + " keeps full width at " +
                    std::to_string(w));
      }
    }
  }

  // Only past the compressed floor does dropping start, and then in priority
  // order: PCI-ID first, then SCLK/MCLK, Fan, power. The floor is derived from
  // the layout rather than hardcoded, so tightening a column moves these with
  // it instead of silently going stale.
  const int compressedFloor = compressedWidth(defaultColumns());
  check(compressedFloor < rowsTotalWidth(defaultColumns()),
        "compression actually gains width");

  std::vector<Column> tiny = fitColumns(defaultColumns(), compressedFloor - 1);
  check(tiny.size() == defaultColumns().size() - 1,
        "first drop happens below the compressed floor");
  check(!hasKey(tiny, "device_id"), "PCI-ID dropped first");

  const int afterPciId = rowsTotalWidth(tiny);
  std::vector<Column> tiny2 = fitColumns(defaultColumns(), afterPciId - 1);
  check(tiny2.size() == defaultColumns().size() - 2,
        "second column dropped as width shrinks");
  check(!hasKey(tiny2, "clocks"), "SCLK/MCLK dropped second");

  for (const char* key : {"gpu", "name", "temp", "memory", "util"}) {
    std::vector<Column> narrowest = fitColumns(defaultColumns(), 20);
    check(hasKey(narrowest, key), "essential column never dropped");
  }

  // The trimmed table must actually fit the target width. Below the essentials
  // floor (gpu, name, temp, memory, util at their minimums) the table
  // deliberately overflows rather than losing the GPU index, so the assertion
  // starts at that floor.
  for (int w : {59, 70, 80, 100, 110}) {
    std::vector<Column> cols = fitColumns(defaultColumns(), w);
    check(rowsTotalWidth(cols) <= w,
          "trimmed table fits " + std::to_string(w));
  }

  // Compression must respect every column's declared minimum, and never invent
  // one: a rigid column keeps its width no matter how narrow the terminal is.
  std::vector<Column> squeezed = fitColumns(defaultColumns(), 90);
  for (const Column& column : squeezed) {
    if (column.minWidth > 0) {
      check(column.width >= column.minWidth,
            "column " + column.key + " respects its minimum width");
    }
  }
}

static void testBoxAlignment() {
  std::printf("BoxRenderer alignment\n");
  std::vector<Column> columns = defaultColumns();
  BoxRenderer box(columns);
  const int width = box.totalWidth();

  checkEq(std::to_string(width), "104", "declared table width");
  check(!hasKey(columns, "type"), "default view has no Type column");
  checkEq(std::to_string(tableWidth(columns)), std::to_string(width),
          "free-function width agrees with the renderer");

  // Offset of the '|' that opens each group, derived by walking the same
  // geometry the renderer uses, so the assertion cannot drift from the layout.
  // The opening corner counts as a boundary too.
  std::vector<int> groupOffsets{0};
  int offset = 1;  // the leading '|'
  for (size_t i = 0; i < columns.size(); ++i) {
    if (i > 0 && columns[i].groupStart) {
      // The boundary '|' is the first character of the separator, so it sits
      // exactly here, before the separator is accounted for.
      groupOffsets.push_back(offset);
      offset += 2;  // "| "
    } else if (i > 0) {
      offset += 1;  // " "
    }
    offset += columns[i].width;
  }
  // Three groups means two internal boundaries plus the opening corner.
  checkEq(std::to_string(groupOffsets.size()), "3",
          "default layout has three groups");

  // Every emitted line must be exactly tableWidth characters, or the borders
  // will not line up in a terminal.
  std::vector<std::string> lines = {
      box.rule('-'),
      box.rule('='),
      box.topRule(),
      box.headerRow(true),
      box.headerRow(false),
      box.banner(" amd-smi-win 0.1.0 | ADLX Version: 1.5.0.0"),
      box.banner(std::string(400, 'x')),
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
  checkEq(std::to_string(lines[8].size()), std::to_string(width),
          "short row pads to full width");

  checkEq(box.rule('-'), box.rule('-'), "rules are deterministic");
  check(box.rule('=') != box.rule('-'), "fill character is honoured");

  // nvidia-smi rules only the group boundaries: '+' at the corners and at the
  // groups, fill everywhere else.
  std::string ruled = box.rule('-');
  for (int groupOffset : groupOffsets) {
    check(ruled[groupOffset] == '+', "rule has '+' at each group boundary");
  }
  check(ruled.front() == '+' && ruled.back() == '+', "rule corners are '+'");
  check(!contains(ruled, "|-"), "rule carries no cell separator artefacts");

  // The top rule is solid: corners only, no junctions at the group boundaries
  // that the ruled header carries. The opening corner is offset 0 and is '+'
  // on both, so it is skipped here.
  std::string top = box.topRule();
  check(top.front() == '+' && top.back() == '+', "top rule corners are '+'");
  for (size_t i = 1; i < groupOffsets.size(); ++i) {
    check(top[groupOffsets[i]] == '-', "top rule is solid, with no junctions");
  }

  // The banner is prose: it clips to fit but is never marked with '~'.
  check(!contains(lines[6], "~"), "overlong banner is not marked truncated");

  // Data rows carry a '|' where a rule has a '+', so the three blocks line up.
  std::string row = box.renderRow({"0", "Radeon RX 9070", "0x744C"});
  for (int groupOffset : groupOffsets) {
    check(row[groupOffset] == '|', "row has '|' at each group boundary");
  }

  check(box.headerRow(true) != box.headerRow(false),
        "top and bottom headers differ");

  // Rows are resolved by column key rather than position, so a superset map
  // renders only the columns that survived the width selection.
  std::string wide = captureStdout([] {
    DefaultView view(200);
    std::printf("%s\n", view.staticRow(0, StaticInfo()).c_str());
    std::printf("%s\n", view.metricRow(Sample(), StaticInfo()).c_str());
  });
  check(contains(wide, "|0"), "GPU index rendered");
  check(!contains(wide, "Type"), "no Type column rendered");
  check(contains(wide, "N/A/N/A"), "missing power renders both halves N/A");

  std::string out = captureStdout([] {
    BoxRenderer box2(defaultColumns());
    std::printf("%s\n", box2.rule('=').c_str());
    std::printf("%s\n", box2.headerRow(true).c_str());
    std::printf("%s\n", box2.headerRow(false).c_str());
    std::printf("%s\n", box2.renderRow({"0", "Radeon RX 9070", "0x744C"})
                            .c_str());
    std::printf("%s\n",
                box2.renderRow({"", "", "", "41C", "2340RPM", "15W/450W",
                                "1024/16384MiB", "0%",
                                "1200/14000MHz"})
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
  sample.fanOk = true;
  sample.fanRpm = 2340;
  sample.sclkOk = sample.mclkOk = true;
  sample.sclkMhz = 1200;
  sample.mclkMhz = 14000;

  // At 105 columns only the PCI-ID gives way; every reading stays. The compact
  // layout means a normal console window loses nothing.
  DefaultView tight(105);
  std::string tightRow = tight.metricRow(sample, info);
  check(tightRow.size() <= 105, "row fits a 105 column window");
  check(contains(tightRow, "15W/450W"), "power survives at 105 columns");
  check(contains(tightRow, "1024/16384MiB"), "memory survives at 105 columns");
  check(contains(tightRow, "2340RPM"), "fan survives at 105 columns");
  check(contains(tightRow, "14000MHz"), "clocks survive at 105 columns");
  check(!contains(tightRow, "PCI-ID"), "PCI-ID is the column given up first");

  // At 78 columns clocks and fan go, but the readings that remain are never
  // truncated with the '~' marker.
  DefaultView narrow(78);
  std::string row = narrow.metricRow(sample, info);
  check(row.size() <= 78, "narrow row fits the window");
  check(contains(row, "15W/450W"), "power survives at 78 columns");
  check(contains(row, "16384MiB"), "total VRAM from TotalVRAM (MiB)");
  check(!contains(row, "~"), "no reading is truncated at 78 columns");

  DefaultView wide(200);
  std::string full = wide.metricRow(sample, info);
  check(contains(full, "15W/450W"), "usage cap and limit rendered");
  check(contains(full, "1024/16384MiB"), "memory shown at full width");
  check(!contains(full, "Radeon RX 9070") && !contains(full, "| 0   |"),
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

static void testNvidiaSmiFrame() {
  std::printf("nvidia-smi frame\n");

  DefaultView view(200);
  StaticInfo info;
  info.name = "Radeon RX 9070 XT";
  info.pciBusIdKnown = true;
  info.pciBusId = "01:00.0";

  Sample sample;
  sample.tempOk = sample.fanOk = sample.powerOk = sample.vramOk = true;
  sample.usageOk = sample.sclkOk = sample.mclkOk = true;
  sample.temp = 48;
  sample.fanRpm = 2100;
  sample.power = 63.4;
  sample.vramUsedMiB = 2317;
  sample.usage = 37;
  sample.sclkMhz = 2400;
  sample.mclkMhz = 14000;

  const std::string top = view.topRule();
  const std::string banner = view.banner({info}, "1.24.0.30000");
  const std::string bannerRule = view.headerRule('-');
  const std::string staticHead = view.staticHeader();
  const std::string metricHead = view.metricHeader();
  const std::string headerRule = view.headerRule('=');
  const std::string staticLine = view.staticRow(0, info);
  const std::string metricLine = view.metricRow(sample, info);
  const std::string bottom = view.bottomRule();

  const std::string width = std::to_string(view.totalWidth());
  std::vector<std::string> lines = {top,       banner, bannerRule, staticHead,
                                    metricHead, headerRule, staticLine,
                                    metricLine, bottom};
  for (const std::string& line : lines) {
    checkEq(std::to_string(line.size()), width,
            "every line of the frame is the table width");
  }

  // The banner is bounded by '|' on every line, rules by '+' at the corners.
  for (const std::string& line : {top, banner, bannerRule, staticHead,
                                  metricHead, headerRule, staticLine,
                                  metricLine, bottom}) {
    check(line.back() == '|' || line.back() == '+', "frame line is closed");
  }

  // The header/data boundary is '=' in nvidia-smi; the banner rules are '-'.
  check(bannerRule.find('=') == std::string::npos,
        "banner rule is not the '=' header boundary");
  check(headerRule.find('=') != std::string::npos,
        "'=' rule separates the headers from the data");
  check(top.find('=') == std::string::npos, "top rule is a solid '-' span");

  // Three groups: the '|' offsets must match the '+' offsets of the ruled line,
  // and the top rule must not have junctions there.
  std::vector<int> bars;
  for (size_t i = 0; i < staticLine.size(); ++i) {
    if (staticLine[i] == '|') {
      bars.push_back(static_cast<int>(i));
    }
  }
  check(bars.size() == 4, "three groups means four '|' per row");
  for (int bar : bars) {
    check(headerRule[size_t(bar)] == '+', "rule '+' aligns with row '|'");
    check(bannerRule[size_t(bar)] == '+', "banner rule '+' aligns too");
  }
  // The first and last '|' are the row's own corners and match the '+' corners;
  // only the internal ones are group boundaries, and those stay solid on the
  // top rule.
  for (size_t i = 1; i + 1 < bars.size(); ++i) {
    check(top[size_t(bars[i])] == '-', "top rule stays solid at boundaries");
  }

  // The header rows carry the identity and telemetry halves of the labels.
  check(contains(staticHead, "GPU") && contains(staticHead, "Name") &&
            contains(staticHead, "PCI-ID"),
        "identity labels in the top header row");
  check(contains(metricHead, "Temp") && contains(metricHead, "Pwr:Usage/Cap") &&
            contains(metricHead, "Memory-Usage") && contains(metricHead, "GPU-Util"),
        "telemetry labels in the bottom header row");

  // The device name is never squeezed in the full-width layout, the way
  // nvidia-smi shows it in full.
  check(contains(staticLine, "Radeon RX 9070 XT"),
        "full device name shown without truncation");
  check(!contains(staticLine, "~"), "no truncation in the static row");
  check(!contains(metricLine, "~"), "no truncation in the metric row");

  // Every reading appears in full.
  for (const char* reading : {"48C", "2100RPM", "63.4W", "2317", "37%",
                              "2400/14000MHz"}) {
    check(contains(metricLine, reading),
          std::string("reading ") + reading + " rendered in full");
  }

  // The timestamp is a line of its own, above the table.
  std::string stamp = view.timestamp();
  check(!stamp.empty(), "timestamp is rendered");
  check(stamp.find('|') == std::string::npos,
        "timestamp carries no table border");
  check(!contains(view.banner({info}, "1.24.0.30000"), stamp),
        "timestamp is not folded into the banner");
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
  testNvidiaSmiFrame();

  std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
