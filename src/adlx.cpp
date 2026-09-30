#include "adlx.h"

#include <cstdio>
#include <cstring>

#include "ADLXHelper.h"

namespace {

// ADLXHelper is a process-wide singleton in AMD's design; the samples declare
// it as a global and reuse it. Keep the same shape.
ADLXHelper g_helper;

}  // namespace

AdlxSession::~AdlxSession() { shutdown(); }

bool AdlxSession::init(std::string& error) {
  ADLX_RESULT res = g_helper.Initialize();
  if (ADLX_FAILED(res)) {
    error =
        "failed to initialize ADLX. Is amdadlx64.dll present? It ships with the "
        "AMD Software: Adrenalin Edition driver (C:\\Windows\\System32).";
    return false;
  }

  m_initialized = true;
  m_fullVersion = g_helper.QueryFullVersion();
  m_system = g_helper.GetSystemServices();

  if (m_system == nullptr) {
    error = "ADLX initialized but returned no system services interface";
    shutdown();
    return false;
  }

  IADLXGPUListPtr gpus;
  if (ADLX_FAILED(m_system->GetGPUs(&gpus))) {
    error = "failed to enumerate GPUs via ADLX";
    shutdown();
    return false;
  }

  for (adlx_uint i = gpus->Begin(); i != gpus->End(); ++i) {
    IADLXGPUPtr gpu;
    if (ADLX_SUCCEEDED(gpus->At(i, &gpu)) && gpu != nullptr) {
      m_gpus.push_back(gpu);
    }
  }

  if (ADLX_FAILED(m_system->GetPerformanceMonitoringServices(&m_perf))) {
    error = "failed to obtain performance monitoring services";
    shutdown();
    return false;
  }

  // Tuning services are optional: integrated GPUs and some laptops do not
  // expose manual tuning. Their absence only costs us the power cap column.
  m_tuning = nullptr;
  m_system->GetGPUTuningServices(&m_tuning);

  return true;
}

void AdlxSession::shutdown() {
  if (m_initialized) {
    g_helper.Terminate();
    m_initialized = false;
  }
  m_system = nullptr;
  m_perf = nullptr;
  m_tuning = nullptr;
  m_gpus.clear();
}

std::string AdlxSession::version() const {
  const char* v = g_helper.QueryVersion();
  return (v != nullptr) ? std::string(v) : std::string("unknown");
}
