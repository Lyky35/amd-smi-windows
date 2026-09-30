#pragma once

#include "adlx.h"
#include "cli.h"

int cmdDefault(AdlxSession& session, const Options& opts);
int cmdStatic(AdlxSession& session, const Options& opts);
int cmdMetric(AdlxSession& session, const Options& opts);

// Shared helpers.
const char* gpuVendorName(const char* vendorId);
std::string gpuAsicFamilyName(ADLX_ASIC_FAMILY_TYPE type);
std::string gpuTypeName(ADLX_GPU_TYPE type);

// Resolves opts.gpus into concrete indices into session.gpus().
// Empty selection means "every GPU".
bool resolveGpuSelection(const Options& opts, const AdlxSession& session,
                         std::vector<size_t>& indices, std::string& error);
