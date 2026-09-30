#pragma once

#include <map>
#include <string>
#include <vector>

// A single column in a boxed table. A column is labelled on the
// header's top row (static data) or bottom row (metric data), or both.
struct Column {
  // Stable identifier, used to look values up by name so that a responsive
  // layout can drop columns without the caller tracking positions.
  std::string key;
  std::string topLabel;
  std::string bottomLabel;
  int width = 0;
  bool rightAlign = false;

  int totalWidth() const { return width + 3; }  // "| " + content + " |"
};

// Draws fixed-width boxed tables. Every emitted line is exactly totalWidth()
// characters wide, including the leading and trailing '+' / '|'.
class BoxRenderer {
 public:
  explicit BoxRenderer(std::vector<Column> columns);

  // Full table width, including both border characters.
  int totalWidth() const;

  // Horizontal rule. `ch` is the fill. With junctions, a '+' is drawn at each
  // listed offset instead of 'ch'.
  std::string rule(char ch) const;
  std::string rule(char ch, const std::vector<int>& junctions) const;

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
  // visible character with '~'.
  static std::string fit(const std::string& text, int width, bool rightAlign);

 private:
  std::vector<Column> m_columns;
};
