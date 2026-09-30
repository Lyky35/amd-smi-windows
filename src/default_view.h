#pragma once

#include <string>
#include <vector>

#include "box.h"
#include "console.h"
#include "gpu_info_types.h"

// Builds the default dashboard: a two-row (static + metric) table per GPU
// under a version banner. The column set adapts to `terminalWidth`.
class DefaultView {
 public:
  // `terminalWidth` of 0 means "unknown"; the full column set is used and the
  // table is not truncated.
  explicit DefaultView(int terminalWidth = detectConsoleWidth());

  // Date and time, printed on its own line above the table as nvidia-smi does.
  std::string timestamp() const;
  std::string banner(const std::vector<StaticInfo>& gpus,
                     const std::string& adlxVersion) const;
  std::string staticHeader() const;
  std::string metricHeader() const;
  std::string staticRow(int index, const StaticInfo& info) const;
  std::string metricRow(const Sample& sample, const StaticInfo& info) const;
  std::string rule(char fill) const;
  // Rules for the three horizontal lines of the nvidia-smi frame: the banner's
  // top edge, the banner/header boundary, the header/data boundary ('=') and the
  // bottom edge. Each carries '+' at the corners and the group boundaries.
  std::string topRule() const;
  std::string headerRule(char fill) const;
  std::string bottomRule() const;

  int totalWidth() const;

  // Effective column set, exposed for tests.
  const std::vector<Column>& columns() const { return m_columns; }

 private:
  std::vector<Column> m_columns;
};
