#pragma once

#include <map>
#include <string>
#include <vector>

// A single column in an nvidia-smi style table. A column is labelled on the
// header's top row (static data) or bottom row (metric data), or both.
struct Column {
  // Stable identifier, used to look values up by name so that a responsive
  // layout can drop columns without the caller tracking positions.
  std::string key;
  std::string topLabel;
  std::string bottomLabel;
  int width = 0;
  bool rightAlign = false;
  // Narrowest this column may become while shrinking instead of being dropped.
  // 0 means the column is rigid and only ever dropped, never compressed.
  int minWidth = 0;
  // Draw a '|' immediately before this column. nvidia-smi groups its table into
  // blocks (identity, telemetry, per-device state) and rules only the block
  // boundaries; columns inside a block are separated by spaces.
  bool groupStart = true;

  bool shrinkable() const { return minWidth > 0 && width > minWidth; }

  // Columns are laid out as "| cell  cell | cell |": a group boundary costs a
  // '|' plus a space before its column, whereas two columns of the same group
  // are separated by a single space. The first column never pays for a leading
  // separator.
  static int leadingSep(const Column& column, bool first) {
    return (!first && column.groupStart) ? 2 : (first ? 0 : 1);
  }
};

// Width of the table drawn from `columns`, in characters. This is the single
// source of truth for the geometry: BoxRenderer measures its rows with the same
// rules, and the responsive layout asks this how much room it needs, so the
// two can never disagree about how wide a table is.
int tableWidth(const std::vector<Column>& columns);
// Draws fixed-width boxed tables. Every emitted line is exactly tableWidth()
// characters wide, including the leading and trailing '|'.
class BoxRenderer {
 public:
  explicit BoxRenderer(std::vector<Column> columns);

  // Full table width, including both border characters.
  int totalWidth() const;

  // Horizontal rule. `ch` is the fill. A '+' is drawn at the corners and at the
  // group boundaries, so the table reads as a few blocks, the way nvidia-smi
  // rules only the block edges rather than every cell.
  std::string rule(char ch) const;

  // Solid rule with '+' at the corners only. nvidia-smi opens its table with one
  // of these before the banner, and the group junctions appear on the rule that
  // closes the banner instead.
  std::string topRule() const;

  // One data row. `cells` may be shorter than the column count; missing
  // entries render as blanks.
  std::string renderRow(const std::vector<std::string>& cells) const;

  // One data row, with each cell resolved by its column key. Keys absent from
  // `values` render as blanks, so the caller can pass a superset of values and
  // let the column selection decide what actually appears.
  std::string renderRow(const std::map<std::string, std::string>& values) const;

  // Header row using either the top or bottom labels.
  std::string headerRow(bool top) const;

  // A single full-width bordered line containing `text`.
  std::string banner(const std::string& text) const;

  // Pads or truncates `text` to exactly `width`. Truncation replaces the last
  // visible character with '~', matching nvidia-smi's convention.
  static std::string fit(const std::string& text, int width, bool rightAlign);

 private:
  std::vector<Column> m_columns;
};
