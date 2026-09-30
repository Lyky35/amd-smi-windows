#include <cstdio>
#include <string>

#include "adlx.h"
#include "cli.h"
#include "commands.h"
#include "output.h"

namespace {

int cmdVersion(AdlxSession& session, const Options& opts) {
  if (opts.format == OutputFormat::Json) {
    JsonBuilder json(OutputFormat::Json);
    json.beginObject("");
    json.field("tool", "amd-smi-win");
    json.field("tool_version", kAppVersion);
    json.field("backend", "ADLX");
    json.field("adlx_version", session.version());
    json.field("adlx_full_version", static_cast<long long>(session.fullVersion()));
    json.field("gpu_count", static_cast<long long>(session.gpus().size()));
    json.endObject();
    json.finish();
    return 0;
  }

  if (opts.format == OutputFormat::Csv) {
    std::printf("field,value\n");
    std::printf("tool,amd-smi-win\n");
    std::printf("tool_version,%s\n", kAppVersion);
    std::printf("backend,ADLX\n");
    std::printf("adlx_version,%s\n", session.version().c_str());
    std::printf("adlx_full_version,%llu\n",
                static_cast<unsigned long long>(session.fullVersion()));
    std::printf("gpu_count,%zu\n", session.gpus().size());
    return 0;
  }

  std::printf("AMD System Management Interface (Windows / ADLX backend)\n");
  std::printf("  tool version    : %s\n", kAppVersion);
  std::printf("  backend         : ADLX\n");
  std::printf("  ADLX version    : %s\n", session.version().c_str());
  std::printf("  GPUs detected   : %zu\n", session.gpus().size());
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  std::string command;
  Options opts;

  if (parseArgs(argc, argv, command, opts) != 0) {
    printUsage();
    return 1;
  }

  if (opts.help) {
    printUsage();
    return 0;
  }

  AdlxSession session;
  std::string error;
  if (!session.init(error)) {
    std::fprintf(stderr, "error: %s\n", error.c_str());
    return 1;
  }

  if (command == "version") {
    return cmdVersion(session, opts);
  }

  if (session.gpus().empty()) {
    std::fputs("error: no AMD GPUs reported by the driver\n", stderr);
    return 1;
  }

  if (opts.verbose) {
    std::fprintf(stderr, "ADLX %s, %zu GPU(s) enumerated\n",
                 session.version().c_str(), session.gpus().size());
  }

  // No subcommand: nvidia-smi style all-in-one dashboard.
  if (command.empty()) {
    return cmdDefault(session, opts);
  }
  if (command == "static") {
    return cmdStatic(session, opts);
  }
  return cmdMetric(session, opts);
}
