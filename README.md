# DES Framework

A C11 library for discrete-event simulation with JSON-based configuration, FSM-driven stages, resource management, and rich CLI output.

## Build

Requires CMake 3.14+ and a C11 compiler (MSVC, GCC, Clang).

```bash
# Configure
cmake -S . -B build

# Build (Release)
cmake --build build --config Release

# Run tests
ctest --test-dir build/tests -C Release --output-on-failure
```

Tests are built by default (`BUILD_TESTS=ON`). To disable: `cmake -S . -B build -DBUILD_TESTS=OFF`.

## Running

```bash
# Run a simulation from JSON config (prints rich summary report)
./build/apps/coffeesim/Release/coffeesim.exe configs/coffee_shop.json

# Run with any config
./build/apps/config_runner/Release/config_runner.exe configs/airport_security.json

# TUI config editor (interactive config builder)
./build/apps/tui_editor/Release/tui_editor.exe configs/coffee_shop.json

# Export to MDF4 format
./build/apps/mdf_export/Release/mdf_export.exe configs/coffee_shop.json output/resources.mf4
```

## Quick Start

### 1. Write a JSON config (recommended)

Using the `"mode": "resource"` shorthand — just specify the resource, processing time, and outcomes:

```json
{
  "simulation": { "max_time": 100000, "seed": 42 },
  "resources": [
    { "name": "Server", "count": 3 }
  ],
  "stages": [
    {
      "name": "Process",
      "mode": "resource",
      "resource": "Server",
      "processing_time": { "distribution": "exponential", "param1": 0.05 },
      "outcomes": [
        { "name": "FINISH", "probability": 1.0 }
      ]
    }
  ],
  "entity_arrivals": [
    { "name": "Job", "count": 200, "entry_stage": "Process",
      "inter_arrival": { "distribution": "exponential", "param1": 0.01 } }
  ]
}
```

### 2. Or build configs programmatically (C API)

```c
#include "des/des.h"

DesSimConfig cfg = DesConfig_init();

int res = DesConfig_addResource(&cfg, "Server", 3);

int stage = DesConfig_addStage(&cfg, "Process");
DesStage_setResource(&cfg, stage, res);
DesStage_setProcessingTime(&cfg, stage, DES_DIST_EXP(0.05));
int finish = DesStage_addOutcome(&cfg, stage, 1.0, NULL, 0, "FINISH");
DesStage_setResourceMode(&cfg, stage, res);

DesConfig_addArrival(&cfg, "Job", 200, "Process", "ENTER", DES_DIST_EXP(0.01));
DesConfig_setSeed(&cfg, 42);

DesEngine *engine = DesEngine_create(&cfg);
DesEngine_run(engine);
DesStats_printSummary(engine);
DesStats_generateReport(engine);
DesEngine_destroy(engine);
```

## Project Structure

```
├── framework/              Core DES library (static lib: des_framework)
│   ├── include/des/        Public headers
│   └── src/                Implementation
├── apps/                   Executables
│   ├── coffeesim/          Coffee shop demo runner
│   ├── config_runner/      Generic JSON config runner
│   ├── tui_editor/         TUI interactive config editor
│   ├── release_pipeline/   Release pipeline demo runner
│   └── mdf_export/         MDF4 file exporter
├── configs/                Example JSON simulation configs
├── tests/                  Unit tests (Unity framework)
└── output/                 Generated CSV/MF4 output
```

## Architecture

See [docs/architecture.md](docs/architecture.md) for detailed software architecture documentation.

## JSON Config Reference

| Section | Description |
|---------|-------------|
| `simulation` | `max_time`, `max_events`, `seed` |
| `resources` | `name`, `count`, optional `available_at` |
| `stages` | `name`, `resource`, `processing_time`, `outcomes[]` (+ optional `mode`, `states[]`, `event_types[]`, `fsm[]`) |
| `entity_arrivals` | `name`, `count`, `entry_stage`, `inter_arrival`, optional `start_time`, `priority` |
| `statistics` | `record_events`, `record_entity_flow`, `record_resource_util`, `output_dir` |

### Stage Modes

- **`"mode": "resource"`** (recommended): Auto-generates IDLE/BUSY states, ENTER/COMPLETE events, and acquire/release FSM. Just specify `resource`, `processing_time`, and `outcomes`.
- **`"mode": "manual"`** (default): Explicit `states[]`, `event_types[]`, `fsm[]` arrays.

### FSM Transitions (manual mode)

Each transition maps `(state, event) -> (next_state, action)`:

| Action | Description |
|--------|-------------|
| `acquire_and_process` | Acquire a resource instance, run processing time, schedule completion |
| `release_and_dispatch` | Release resource, pick outcome, dispatch to next stage or exit |
| `release_and_retry` | Release resource and retry after delay |
| `wait_retry` | Wait and retry without releasing |
| `none` | No action |

### Distributions

| Type | Parameters |
|------|-----------|
| `fixed` | `param1` = value |
| `uniform` | `param1` = min, `param2` = max |
| `exponential` | `param1` = lambda |
| `normal` | `param1` = mean, `param2` = stddev |

### Error Handling

- `DesConfig_getLastError(cfg)` — Builder error details after failed add operations
- `DesConfig_getLoadError()` — JSON parse error details after failed loads
- `DesError_toString(code)` — Human-readable error code strings
