#pragma once

#include <string>
#include <vector>

enum class OutputFormat { Table, Json, Csv };

struct Options {
  OutputFormat format = OutputFormat::Table;

  // Empty means "all GPUs".
  std::vector<int> gpus;

  // Sampling options for `metric`.
  int intervalMs = 0;   // 0 = single shot
  int iterations = 1;

  // Explicit terminal width for the default view (0 = detect from console).
  int width = 0;

  bool verbose = false;
  bool help = false;
};

// Parses argv into `opts` and resolves the subcommand into `command`.
// Returns 0 on success, non-zero on a usage error (message already printed).
int parseArgs(int argc, char** argv, std::string& command, Options& opts);

void printUsage();
