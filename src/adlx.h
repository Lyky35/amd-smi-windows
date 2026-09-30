#pragma once

#include <string>
#include <vector>

#include "ADLX.h"
#include "ISystem.h"
#include "IPerformanceMonitoring.h"
#include "IGPUTuning.h"

// Thin RAII wrapper over ADLXHelper. Owns initialization and GPU enumeration
// so the command implementations can stay focused on presentation.
class AdlxSession {
 public:
  AdlxSession() = default;
  ~AdlxSession();

  AdlxSession(const AdlxSession&) = delete;
  AdlxSession& operator=(const AdlxSession&) = delete;

  bool init(std::string& error);
  void shutdown();

  IADLXSystem* system() const { return m_system; }
  IADLXPerformanceMonitoringServices* perfMonitoring() const { return m_perf; }
  IADLXGPUTuningServices* gputuning() const { return m_tuning; }
  // ADL<->ADLX bridge; used for the PCI bus:device.function lookup.
  IADLMapping* mapping() const { return m_mapping; }

  // ADLX version string, e.g. "1.5.0.0".
  std::string version() const;

  // Full (64-bit) version packed value.
  adlx_uint64 fullVersion() const { return m_fullVersion; }

  const std::vector<IADLXGPU*>& gpus() const { return m_gpus; }

 private:
  bool m_initialized = false;
  IADLXSystem* m_system = nullptr;
  IADLXPerformanceMonitoringServices* m_perf = nullptr;
  IADLXGPUTuningServices* m_tuning = nullptr;
  IADLMapping* m_mapping = nullptr;
  adlx_uint64 m_fullVersion = 0;
  std::vector<IADLXGPU*> m_gpus;
};
