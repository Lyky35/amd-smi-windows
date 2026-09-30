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
// windowMs-long history so the reported figures are a true average rather than
// a single instantaneous acquisition (which can read 0% util and miss the power
// draw). Falls back to readSample when history tracking is unavailable.
//
// `samplesAveraged`, when given, receives the number of history samples that
// contributed to the averages. Zero means the driver returned no usable history
// and the figures are a single instantaneous read, which is the difference
// between a real 0% and a missed one. Reported under --verbose.
bool readWindowedSample(AdlxSession& session, IADLXGPU* gpu,
                        const Capabilities& caps, Sample& sample,
                        int windowMs = 1000, int* samplesAveraged = nullptr);