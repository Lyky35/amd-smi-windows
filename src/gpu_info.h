#pragma once

#include "ADLX.h"
#include "ISystem.h"
#include "ISystem2.h"
#include "IPerformanceMonitoring.h"
#include "IGPUTuning.h"
#include "IGPUManualPowerTuning.h"
#include "gpu_info_types.h"

// ADLX-specific data collection. The structs themselves live in
// gpu_info_types.h so the dashboard view stays ADLX-free and testable on any
// host; this header only holds the functions that talk to the ADLX runtime.

class AdlxSession;

void readStaticInfo(AdlxSession& session, IADLXGPU* gpu, StaticInfo& info);
Capabilities readCapabilities(AdlxSession& session, IADLXGPU* gpu);
bool readSample(AdlxSession& session, IADLXGPU* gpu, const Capabilities& caps,
                Sample& sample);

// Like readSample, but averages utilization and power over a rolling
// windowMs-long history so the reported figures match what nvidia-smi shows
// (instantaneous acquisitions can read 0% util and miss the power draw). Falls
// back to readSample when history tracking is unavailable.
bool readWindowedSample(AdlxSession& session, IADLXGPU* gpu,
                        const Capabilities& caps, Sample& sample,
                        int windowMs = 1000);