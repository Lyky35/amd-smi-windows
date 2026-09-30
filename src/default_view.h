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

  std::string banner(const std::vector<StaticInfo>& gpus,
                     const std::string& adlxVersion) const;
  std::string staticHeader() const;
  std::string metricHeader() const;
  std::string staticRow(int index, const StaticInfo& info) const;
  std::string metricRow(const Sample& sample, const StaticInfo& info) const;
  std::string rule(char fill) const;
  std::string topRule() const;
  std::string bottomRule() const;

  int totalWidth() const;

  // Effective column set, exposed for tests.
  const std::vector<Column>& columns() const { return m_columns; }

 private:
  std::vector<Column> m_columns;
};
