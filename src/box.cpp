#include "box.h"

#include <algorithm>

BoxRenderer::BoxRenderer(std::vector<Column> columns)
    : m_columns(std::move(columns)) {}

int BoxRenderer::totalWidth() const {
  int width = 1;
  for (const Column& column : m_columns) {
    width += column.totalWidth();
  }
  return width;
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
  std::string line = "+";
  for (const Column& column : m_columns) {
    // totalWidth() already counts the trailing separator, so fill one less.
    line.append(static_cast<size_t>(column.totalWidth() - 1), ch);
    line += '+';
  }
  return line;
}

std::string BoxRenderer::rule(char ch, const std::vector<int>& junctions) const {
  std::string line = "+";
  for (const Column& column : m_columns) {
    // totalWidth() already counts the trailing separator, so fill one less.
    line.append(static_cast<size_t>(column.totalWidth() - 1), ch);
    // The character about to be appended sits at the current string length.
    int index = static_cast<int>(line.size());
    bool isJunction = std::find(junctions.begin(), junctions.end(), index) !=
                      junctions.end();
    line += isJunction ? '+' : ch;
  }
  return line;
}

std::string BoxRenderer::renderRow(const std::vector<std::string>& cells) const {
  std::string line = "|";
  for (size_t i = 0; i < m_columns.size(); ++i) {
    const std::string& cell = (i < cells.size()) ? cells[i] : std::string();
    line += " " + fit(cell, m_columns[i].width, m_columns[i].rightAlign) + " |";
  }
  return line;
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
  std::vector<std::string> labels;
  labels.reserve(m_columns.size());
  for (const Column& column : m_columns) {
    labels.push_back(top ? column.topLabel : column.bottomLabel);
  }
  return renderRow(labels);
}

std::string BoxRenderer::banner(const std::string& text) const {
  int width = totalWidth() - 2;  // exclude the two border characters
  return "|" + fit(text, width, false) + "|";
}
