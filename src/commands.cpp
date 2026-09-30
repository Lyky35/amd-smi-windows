#include "commands.h"

#include <algorithm>
#include <cstdio>

const char* gpuVendorName(const char* vendorId) {
  if (vendorId == nullptr) {
    return "N/A";
  }
  if (std::strcmp(vendorId, "0x1002") == 0 || std::strcmp(vendorId, "AMD") == 0) {
    return "AMD";
  }
  if (std::strcmp(vendorId, "0x10de") == 0 || std::strcmp(vendorId, "NVIDIA") == 0) {
    return "NVIDIA";
  }
  if (std::strcmp(vendorId, "0x8086") == 0 || std::strcmp(vendorId, "Intel") == 0) {
    return "Intel";
  }
  return vendorId;
}

std::string gpuAsicFamilyName(ADLX_ASIC_FAMILY_TYPE type) {
  switch (type) {
    case ASIC_UNDEFINED:  return "UNDEFINED";
    case ASIC_RADEON:     return "RADEON";
    case ASIC_FIREPRO:    return "FIREPRO";
    case ASIC_FIREMV:     return "FIREMV";
    case ASIC_FIRESTREAM: return "FIRESTREAM";
    case ASIC_FUSION:     return "FUSION";
    case ASIC_EMBEDDED:   return "EMBEDDED";
    default:              return "OTHER";
  }
}

std::string gpuTypeName(ADLX_GPU_TYPE type) {
  switch (type) {
    case GPUTYPE_UNDEFINED:  return "UNDEFINED";
    case GPUTYPE_INTEGRATED: return "INTEGRATED";
    case GPUTYPE_DISCRETE:   return "DISCRETE";
    default:                 return "OTHER";
  }
}

bool resolveGpuSelection(const Options& opts, const AdlxSession& session,
                         std::vector<size_t>& indices, std::string& error) {
  const size_t total = session.gpus().size();

  if (opts.gpus.empty()) {
    indices.resize(total);
    for (size_t i = 0; i < total; ++i) {
      indices[i] = i;
    }
    return true;
  }

  // De-duplicate while preserving the user's ordering.
  std::vector<int> seen;
  for (int requested : opts.gpus) {
    if (requested < 0 || static_cast<size_t>(requested) >= total) {
      char buf[160];
      std::snprintf(buf, sizeof(buf),
                    "error: GPU index %d is out of range (0-%zu)", requested,
                    total == 0 ? 0 : total - 1);
      error = buf;
      return false;
    }
    if (std::find(seen.begin(), seen.end(), requested) == seen.end()) {
      seen.push_back(requested);
    }
  }

  indices.assign(seen.begin(), seen.end());
  return true;
}
