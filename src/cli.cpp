#include "cli.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "output.h"

namespace {

bool parseInt(const char* s, int& out) {
  if (s == nullptr || *s == '\0') {
    return false;
  }
  char* end = nullptr;
  long v = std::strtol(s, &end, 10);
  if (*end != '\0') {
    return false;
  }
  out = static_cast<int>(v);
  return true;
}

void applyGpuSelector(const char* value, Options& opts) {
  // Accepts: "0", "0,1,3", "all", "0-3", and combinations ("all" wins).
  std::string spec(value);
  if (spec == "all") {
    opts.gpus.clear();
    return;
  }

  size_t pos = 0;
  while (pos <= spec.size()) {
    size_t comma = spec.find(',', pos);
    std::string token = spec.substr(
        pos, comma == std::string::npos ? std::string::npos : comma - pos);
    pos = (comma == std::string::npos) ? spec.size() + 1 : comma + 1;
    if (token.empty()) {
      continue;
    }

    size_t dash = token.find('-');
    if (dash != std::string::npos) {
      int lo = 0;
      int hi = 0;
      if (parseInt(token.substr(0, dash).c_str(), lo) &&
          parseInt(token.substr(dash + 1).c_str(), hi)) {
        for (int i = lo; i <= hi; ++i) {
          opts.gpus.push_back(i);
        }
      }
      continue;
    }

    int idx = 0;
    if (parseInt(token.c_str(), idx)) {
      opts.gpus.push_back(idx);
    }
  }
}

}  // namespace

void printUsage() {
  std::puts(
      "AMD System Management Interface (Windows / ADLX backend) | Version: ");
  std::printf("%s\n\n", kAppVersion);
  std::puts(
      "usage: amd-smi [-h] [--json] [--csv] [-v] [<command>] [options]\n"
      "\n"
      "usage: amd-smi [-h] [--json] [--csv] [-v] [<command>] [options]\n"
      "\n"
      "With no command, prints a summary of every GPU.\n"
      "\n"
      "commands:\n"
      "  static        Print static per-GPU device information\n"
      "  metric        Print live per-GPU metrics\n"
      "  version       Print ADLX library and driver versions\n"
      "\n"
      "options:\n"
      "  --gpu LIST          Target GPUs. Accepts: 0 | 0,1,3 | 0-3 | all (default: all)\n"
      "  --json              Emit JSON\n"
      "  --csv               Emit CSV\n"
      "  --width N           Console width override (default view, default: detected)\n"
      "  -i, --interval MS   Sampling period in ms (metric)\n"
      "  -n, --iterations N  Number of samples to collect (metric, default 1)\n"
      "  -v, --verbose       Include extra diagnostics on stderr\n"
      "  -h, --help          Show this help\n"
      "\n"
      "examples:\n"
      "  amd-smi                    summary of every GPU\n"
      "  amd-smi static\n"
      "  amd-smi metric --gpu 0\n"
      "  amd-smi metric -i 1000 -n 10\n"
      "  amd-smi --json --gpu all");
}

int parseArgs(int argc, char** argv, std::string& command, Options& opts) {
  command.clear();

  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i];

    if (std::strcmp(arg, "-h") == 0 || std::strcmp(arg, "--help") == 0) {
      opts.help = true;
      return 0;
    } else if (std::strcmp(arg, "--json") == 0) {
      opts.format = OutputFormat::Json;
    } else if (std::strcmp(arg, "--csv") == 0) {
      opts.format = OutputFormat::Csv;
    } else if (std::strcmp(arg, "-v") == 0 || std::strcmp(arg, "--verbose") == 0) {
      opts.verbose = true;
    } else if (std::strcmp(arg, "--gpu") == 0) {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "error: --gpu requires an argument\n");
        return 1;
      }
      applyGpuSelector(argv[++i], opts);
    } else if (std::strcmp(arg, "--width") == 0) {
      if (i + 1 >= argc || !parseInt(argv[i + 1], opts.width) || opts.width < 20) {
        std::fprintf(stderr, "error: --width requires an integer >= 20\n");
        return 1;
      }
      ++i;
    } else if (std::strcmp(arg, "-i") == 0 || std::strcmp(arg, "--interval") == 0) {
      if (i + 1 >= argc || !parseInt(argv[i + 1], opts.intervalMs)) {
        std::fprintf(stderr, "error: --interval requires a positive integer\n");
        return 1;
      }
      ++i;
    } else if (std::strcmp(arg, "-n") == 0 || std::strcmp(arg, "--iterations") == 0) {
      if (i + 1 >= argc || !parseInt(argv[i + 1], opts.iterations) ||
          opts.iterations < 1) {
        std::fprintf(stderr, "error: --iterations requires an integer >= 1\n");
        return 1;
      }
      ++i;
    } else if (arg[0] == '-') {
      std::fprintf(stderr, "error: unknown option '%s'\n", arg);
      return 1;
    } else if (command.empty()) {
      command = arg;
    } else {
      std::fprintf(stderr, "error: unexpected argument '%s'\n", arg);
      return 1;
    }
  }

  // An empty command is not an error: it selects the default dashboard view.
  if (!command.empty() && command != "static" && command != "metric" &&
      command != "version") {
    std::fprintf(stderr, "error: unknown command '%s'\n", command.c_str());
    return 1;
  }
  return 0;
}
