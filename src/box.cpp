#include "box.h"

#include <algorithm>

BoxRenderer::BoxRenderer(std::vector<Column> columns)
    : m_columns(std::move(columns)) {}

int tableWidth(const std::vector<Column>& columns) {
  // Matches renderRow(): a leading '|' and a trailing '|', plus each column's
  // content and the separator that precedes it.
  int total = 2;
  for (size_t i = 0; i < columns.size(); ++i) {
    total += columns[i].width + Column::leadingSep(columns[i], i == 0);
  }
  return total;
}

int BoxRenderer::totalWidth() const {
  return tableWidth(m_columns);
}

std::string BoxRenderer::fit(const std::string& text, int width, bool rightAlign) {
  if (width <= 0) {
    return std::string();
  }
  std::string s = text;
  if (static_cast<int>(s.size()) > width) {
    s = s.substr(0, width);
    if (width >= 1) {
      s[width - 1] = '~';
    }
  }
  std::string padding(static_cast<size_t>(width) - s.size(), ' ');
  return rightAlign ? padding + s : s + padding;
}

std::string BoxRenderer::rule(char ch) const {
  // nvidia-smi's header rule: '+' at the corners and at the group boundaries,
  // a solid run of `ch` between them. Columns inside a group are not ruled off
  // from each other, so the table reads as three blocks rather than a grid.
  // Walks the same geometry as renderRow(), so a junction lands exactly where
  // the matching '|' lands in the data rows.
  std::string line = "+";
  for (size_t i = 0; i < m_columns.size(); ++i) {
    if (i > 0) {
      // The separator in front of a group-opening column is "| "; in the rule
      // the '|' becomes '+' and the space stays fill.
      if (m_columns[i].groupStart) {
        line += '+';
      }
      line += ch;
    }
    line.append(static_cast<size_t>(m_columns[i].width), ch);
  }
  return line + "+";
}

std::string BoxRenderer::topRule() const {
  // Solid span with '+' at the corners only: nvidia-smi's group junctions start
  // at the rule below the banner, not above it.
  std::string line = "+";
  for (size_t i = 0; i < m_columns.size(); ++i) {
    if (i > 0) {
      line.append(static_cast<size_t>(Column::leadingSep(m_columns[i], false)),
                  '-');
    }
    line.append(static_cast<size_t>(m_columns[i].width), '-');
  }
  return line + "+";
}

std::string BoxRenderer::renderRow(const std::vector<std::string>& cells) const {
  std::string line = "|";
  for (size_t i = 0; i < m_columns.size(); ++i) {
    const std::string& cell = (i < cells.size()) ? cells[i] : std::string();
    if (i > 0) {
      // nvidia-smi separates a group boundary with "| " before the column that
      // opens the group, and two columns of the same group with one space.
      line += m_columns[i].groupStart ? "| " : " ";
    }
    line += fit(cell, m_columns[i].width, m_columns[i].rightAlign);
  }
  return line + "|";
}

std::string BoxRenderer::renderRow(
    const std::map<std::string, std::string>& values) const {
  std::vector<std::string> cells;
  cells.reserve(m_columns.size());
  for (const Column& column : m_columns) {
    auto it = values.find(column.key);
    cells.push_back(it == values.end() ? std::string() : it->second);
  }
  return renderRow(cells);
}

std::string BoxRenderer::headerRow(bool top) const {
  // nvidia-smi centres each header label over its column, so the labels line up
  // with the values below regardless of whether those values are left or right
  // aligned. Labels longer than the column are truncated the same way.
  std::vector<std::string> cells;
  cells.reserve(m_columns.size());
  for (const Column& column : m_columns) {
    const std::string& label = top ? column.topLabel : column.bottomLabel;
    if (label.size() >= static_cast<size_t>(column.width)) {
      cells.push_back(fit(label, column.width, false));
      continue;
    }
    size_t slack = static_cast<size_t>(column.width) - label.size();
    cells.push_back(std::string(slack / 2, ' ') + label);
  }
  return renderRow(cells);
}

std::string BoxRenderer::banner(const std::string& text) const {
  // The banner is prose, not a value: it is padded to the table width and any
  // excess is dropped outright rather than marked with '~', which would read as
  // a truncated device name rather than a truncated summary line.
  int width = totalWidth() - 2;  // exclude the two border characters
  std::string clipped =
      static_cast<int>(text.size()) > width ? text.substr(0, size_t(width))
                                            : text;
  return "|" + clipped +
         std::string(static_cast<size_t>(width) - clipped.size(), ' ') + "|";
}
