# amd-smi-win

An `amd-smi`-compatible command line tool for **Windows**, built on AMD's official
**ADLX** (AMD Device Library eXtra) SDK.

> **Why not the real `amd-smi`?**
> The upstream [AMD SMI](https://github.com/ROCm/rocm-systems/tree/develop/projects/amdsmi)
> project cannot be built for Windows. It is Linux-only by construction: it reads
> `/sys/class/drm`, opens `/dev/dri`, parses `/proc/cpuinfo`, and requires
> `libdrm` / `libdrm_amdgpu` via `pkg_check_modules`. There are no `WIN32` or
> MSVC branches anywhere in its build system. AMD's own documentation still
> describes it as *"Linux Baremetal"* only.
>
> ADLX is the interface the Adrenalin driver actually exposes to user-mode
> applications on Windows, so that is what this tool is built on.
> This project is not affiliated with or endorsed by AMD.

## Features

Run with **no subcommand** for an nvidia-smi style summary of every GPU:

```
Tue Sep 29 22:27:42 2026
+------------------------------------------------------------------------------------------------------+
|amd-smi-win 0.1.1  |  ADLX 1.24.0.30000  |  Driver N/A  |  ROCm N/A (Windows)  |  GPUs 2              |
+-------------------------+------------------------------------------------------------------+---------+
|GPU         Name         |                                                                  |  PCI-ID |
|                         | Temp     Fan Pwr:Usage/Cap    Memory-Usage GPU-Util     SCLK/MCLK|         |
+=========================+==================================================================+=========+
|0   Radeon RX 9070 XT    |                                                                  | 01:00.0 |
|                         |  48C 2100RPM    63.4W/304W   2317/16384MiB      37% 2400/14000MHz|         |
|1   Radeon RX 7900 XTX   |                                                                  | 0b:00.0 |
|                         |  55C    0RPM   104.8W/355W  12043/24576MiB      88% 2450/14000MHz|         |
+-------------------------+------------------------------------------------------------------+---------+
```

The frame follows `nvidia-smi`: a timestamp line, a banner inside a solid rule,
the header split across an identity row and a telemetry row, then a `=` rule
separating the headers from the data. Columns are grouped into three blocks —
identity, telemetry, per-device state — and only the block boundaries are
ruled, so the table reads as blocks rather than a grid of cells.

The banner carries the runtime **ADLX** and Windows **Driver** versions and a
**ROCm** field. ROCm is a Linux-only runtime; on Windows it is reported as
`N/A (Windows)` rather than being left misleadingly blank.

Subcommands give the same data in machine-friendly form:

| Command   | Description                                                        |
| --------- | ------------------------------------------------------------------ |
| `static`  | Per-GPU device info: name, vendor, ASIC family, VRAM, clocks       |
| `metric`  | Live telemetry: utilization, temperature, power, fan, clocks, VRAM  |
| `version` | ADLX library version, driver versions, detected GPU count           |

Output formats: table (default), `--json`, `--csv`.
GPU selection: `--gpu 0`, `--gpu 0,1,3`, `--gpu 0-3`, `--gpu all`.

### Responsive layout

The dashboard adapts to your console width instead of wrapping or truncating
mid-table. It adapts in two stages, so a tight window never costs you a reading
before it costs you a column:

1. **Squeeze.** The device name gives up characters first, marked with `~`
   exactly as nvidia-smi does. The full layout is 104 characters and squeezes to
   100, so an ordinary window shows every statistic in full.
2. **Drop.** Below that, whole columns go in order of expendability:
   `PCI-ID`, `SCLK/MCLK`, `Fan`, then power. The GPU index, name, temperature,
   memory and utilization are never dropped.

A numeric column is never squeezed, because truncating one would mangle the
reading itself (`1200/1400~MHz`) rather than just tightening a label.

Setting `COLUMNS` also works (MSYS2/Cygwin set it automatically), and
`--width N` forces an explicit size for capture or scripting. When output is
redirected to a file or pipe the full, untruncated layout is used.

### Columns the default view omits

The layout mirrors nvidia-smi, but ADLX does not expose everything
`nvidia-smi` reports, so those columns are **omitted rather than faked**:

| nvidia-smi column | Why it is missing here |
| --- | --- |
| Persistence-M | No such concept for AMD GPUs on Windows |
| Perf (P-state) | No performance-state query in ADLX |
| Volatile Uncorr ECC | Not reported for AMD GPUs via ADLX |
| Disp.A / Compute M. | NVIDIA display/compute-mode concepts |

`PCI-ID` takes the place of nvidia-smi's `Bus-Id`, resolved from ADLX's PCI
mapping as `domain:bus:device.function` and falling back to the chip device id
when the lookup fails. The metric row carries `SCLK/MCLK`, which ADLX does
provide.

## Requirements

On the target Windows machine:

- Windows 10 or later, 64-bit
- **AMD Software: Adrenalin Edition** driver. ADLX lives in `amdadlx64.dll`,
  which the driver installs into `C:\Windows\System32`. Nothing else is needed.
- An AMD GPU (Radeon / Instinct)

## Building

Cross-compiled from Linux, or built natively from Visual Studio / MSVC.

The AMD ADLX SDK is **not committed** to this repository (AMD's SDK license
covers distribution of your object code, not of the SDK itself). Fetch it:

```bash
./fetch-adlx-sdk.sh     # clones AMD's official GPUOpen-LibrariesAndSDKs/ADLX into ./vendor/ADLX
```

### Linux cross-compile (no root required)

```bash
./fetch-adlx-sdk.sh     # one-time: fetches the ADLX SDK into ./vendor/ADLX
./fetch-toolchain.sh    # one-time: downloads llvm-mingw into ./llvm-mingw
./build.sh
./build/bin/amd-smi.exe
```

If you already have system MinGW-w64 on `PATH`, skip `fetch-toolchain.sh`:

```bash
sudo pacman -S mingw-w64-gcc cmake   # Arch
./build.sh
```

Override the toolchain location with `MINGW_PREFIX=/path/to/llvm-mingw ./build.sh`.

### Native Windows

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The `Windows.h` case shim in `CMakeLists.txt` is generated automatically and is a
harmless no-op on a case-insensitive filesystem.

## Usage

```
AMD System Management Interface (Windows / ADLX backend) | Version: 0.1.0

usage: amd-smi [-h] [--json] [--csv] [-v] [<command>] [options]

With no command, prints an nvidia-smi style summary of every GPU.

commands:
  static        Print static per-GPU device information
  metric        Print live per-GPU metrics
  version       Print ADLX library and driver versions

options:
  --gpu LIST          Target GPUs: 0 | 0,1,3 | 0-3 | all (default: all)
  --json              Emit JSON
  --csv               Emit CSV
  --width N           Console width override for the default view
  -i, --interval MS   Sampling period in ms (metric)
  -n, --iterations N  Number of samples to collect (metric, default 1)
  -v, --verbose       Extra diagnostics on stderr
  -h, --help          Show help
```

Examples:

```bash
amd-smi                          # nvidia-smi style summary
amd-smi static
amd-smi metric --gpu 0
amd-smi metric -i 1000 -n 10     # 10 samples, 1s apart
amd-smi --json --gpu all > metrics.json
```

Unsupported metrics render as `N/A` (and `null` in JSON) rather than failing the
report — driver capability is queried per GPU via `IADLXGPUMetricsSupport`, and
each value carries its own validity flag.

Data sources for the "Pwr:Usage/Cap" column:

1. **Usage** is the `GPUPower` metric (Watts) from
   `IADLXPerformanceMonitoringServices`.
2. **Cap** is the configured board power limit from
   `IADLXManualPowerTuning::GetPowerLimit` (falling back to
   `GetPowerLimitRange().max` when the scalar read is rejected, e.g. under
   SmartShift). When no manual PowerTuning interface exists but the driver still
   reports a total-board-power range, its ceiling is shown instead. If none of
   the three is available the field degrades to `N/A`.

Total VRAM comes from `IADLXGPU::TotalVRAM` (MB) — not from the metric ranges,
whose upper bounds are not guaranteed to describe the board's capacity.

## Design notes

**ADLX is loaded at runtime, never linked, and never committed.** AMD ships no
import library for `amdadlx64.dll`; `ADLXHelper` resolves it with
`LoadLibraryEx`/`GetProcAddress` at startup. Consequently the compiled
executable imports only `KERNEL32.dll` and the UCRT forwarders built into
Windows 10+, and it stays forward/backward compatible with future driver
versions:

```
$ objdump -p build/bin/amd-smi.exe | grep "DLL Name"
    DLL Name: KERNEL32.dll
    DLL Name: api-ms-win-crt-stdio-l1-1-0.dll
    ...
```

This is also what makes cross-compiling clean: there is no MSVC/MinGW C++ ABI
boundary to bridge, because every ADLX call crosses the DLL boundary through a
C calling convention.

**ADLX SDK is fetched, not vendored.** AMD's SDK lives under AMD's official
[GPUOpen-LibrariesAndSDKs/ADLX](https://github.com/GPUOpen-LibrariesAndSDKs/ADLX)
repository; `./fetch-adlx-sdk.sh` clones it and copies the `SDK/` tree into
`vendor/ADLX/` (git-ignored). The only cross-compilation papercuts are handled
in `CMakeLists.txt` instead of by patching AMD's sources:

- `Windows.h` vs `windows.h` — a generated forwarding header covers the
  case-sensitive host filesystem.
- `__declspec(novtable)` is MSVC-only; clang warns and ignores it. The attribute
  only asks for vtable elision on interfaces ADLX instantiates internally, so
  ignoring it is semantically inert.

## Tests

```bash
./run-tests.sh
```

Compiles and runs the platform-independent logic natively (argument parsing,
GPU selector expansion, numeric/byte formatting, JSON escaping, and the
table/CSV/JSON renderers) against a captured-stdout harness. The ADLX calls
themselves can only be exercised on Windows with a real driver.

## Differences from Linux `amd-smi`

| Feature | Linux amd-smi | This tool |
| --- | --- | --- |
| `static`, `metric`, `version` | yes | yes |
| `process` | yes (reads `/proc`) | **no** — no supported Windows API |
| `--rocm-smi` compat mode | yes | no |
| `set` / `reset` (VRAM, GTT tuning) | yes | not implemented |
| JSON/CSV | yes | yes |

`process` listing is deliberately absent: ADLX exposes no per-process GPU
memory interface, so it cannot be reproduced on Windows. GPU reset and VRAM/GTT
tuning are left out for now; ADLX does expose tuning interfaces
(`IGPUManualVRAMTuning`, `IGPUManualGFXTuning`, ...) if they are wanted later.

## License

This project's source is provided as-is. It intentionally does not include
AMD's ADLX SDK materials; those are fetched by `./fetch-adlx-sdk.sh` from AMD's
official repository and remain under AMD's
[ADLX SDK License Agreement](https://github.com/GPUOpen-LibrariesAndSDKs/ADLX/blob/main/ADLX%20SDK%20License%20Agreement.pdf).

AMD is a trademark of Advanced Micro Devices, Inc. This project is not
affiliated with or endorsed by AMD.
