# FlowLab Discrete-Event Simulation

FlowLab is a C11 discrete-event simulation workbench for answering operational questions such as:

- What is the smallest capacity that meets a service target?
- Which stage creates the most delay?
- How many engineers, testers, benches, or vehicles are useful?
- How do rework probabilities or resource availability change lead time?

Scenarios describe entities moving through interlinked stages with resources, processing-time distributions, state transitions, and probabilistic outcomes. The native engine runs individual simulations, replicated experiments, and resource-capacity sweeps. A dependency-free browser workbench provides visual scenario editing and result comparison.

## Quick start

Requirements: CMake 3.14 or newer and a C11 compiler. Ninja is optional.

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

On single-configuration generators, executables are below `build/apps`. On Visual Studio they may be inside a `Release` subdirectory.

For CMake 3.21 or newer with Ninja, presets provide the same workflow:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

## Run experiments

The `desim` application is the stable interface for people, scripts, and AI agents.

```bash
# Validate a model and receive structured diagnostics
./build/apps/desim/desim validate configs/whatif_release.json --json

# Run one deterministic simulation
./build/apps/desim/desim run configs/whatif_release.json --seed 42 --json

# Export a deterministic replay for the browser workbench
./build/apps/desim/desim run configs/coffee_shop.json \
  --seed 42 --replay replay.json --json

# Find the smallest capacity whose upper flow-time confidence bound meets the target
./build/apps/desim/desim sweep configs/coffee_shop.json \
  --resource Cashier=1:6 --runs 100 --seed 42 \
  --objective 'mean-flow<=100' --json

# Throughput objectives are supported as well
./build/apps/desim/desim sweep configs/coffee_shop.json \
  --resource Cashier=1:6 --runs 100 --seed 42 \
  --objective 'throughput>=0.4' --json
```

Version-2 experiment results include completed count, completion rate, throughput, mean and p95 flow time, makespan, utilization, and 95% confidence intervals. A capacity qualifies only when the conservative confidence bound meets the objective.

## Visual workbench

Open [apps/workbench/index.html](apps/workbench/index.html) in a modern browser. It works directly from the filesystem and requires no package installation or web server.

The workbench supports:

- Importing and exporting scenario JSON
- Adding, removing, and sizing resource pools
- Editing simple stages or opt-in advanced visual FSMs
- Connecting stages with probabilistic transitions and exits
- Immediate structural validation
- Replaying entity flow, FSM transitions, and per-instance resource timelines
- Preparing objective-based capacity-sweep commands and comparing version-2 results

The browser editor and native CLI use the same versioned JSON scenario contract. Native validation remains authoritative.

## Scenario model

The recommended stage mode is `"resource"`. It creates the standard `IDLE → BUSY → IDLE` lifecycle while still allowing probabilistic routing and rework loops.

```json
{
  "format_version": 1,
  "simulation": {
    "max_time": 100000,
    "max_events": 100000,
    "seed": 42
  },
  "resources": [
    { "name": "Test_Engineer", "count": 3, "available_at": 0 }
  ],
  "stages": [
    {
      "name": "System_Test",
      "mode": "resource",
      "resource": "Test_Engineer",
      "processing_time": {
        "distribution": "uniform",
        "param1": 20,
        "param2": 40
      },
      "outcomes": [
        { "name": "PASS", "probability": 0.85, "next_stage": null },
        { "name": "REWORK", "probability": 0.15,
          "next_stage": "System_Test", "next_event": "ENTER" }
      ]
    }
  ],
  "entity_arrivals": [
    {
      "name": "Release",
      "count": 50,
      "entry_stage": "System_Test",
      "inter_arrival": { "distribution": "fixed", "param1": 10 }
    }
  ]
}
```

Supported distributions in the current engine are `fixed`, `uniform`, `exponential`, and `normal`. The canonical schema is [configs/des_schema.json](configs/des_schema.json).

### Manual FSM mode

Advanced stages may define explicit states, events, and transitions. Each transition maps `(state, event)` to a next state and action:

- `acquire_and_process`
- `release_and_dispatch`
- `release_and_retry`
- `wait_retry`
- `entity_enter`
- `entity_exit`
- `custom`
- `none`

The engine infers entry and completion event indices from the configured actions, so manual event ordering is supported.
`initial_state` explicitly selects the starting state and defaults to the first state for existing scenarios. FSM state is entity-scoped; resource occupancy is tracked independently, so resource pools can be shared safely across stages.

## C API

```c
#include "des/des.h"

DesSimConfig *cfg = DesConfig_create();
int team = DesConfig_addResource(cfg, "Test_Engineer", 3);
int stage = DesConfig_addStage(cfg, "System_Test");

DesStage_setResourceMode(cfg, stage, team, DES_DIST_FIXED, 30, 0);
DesStage_addOutcomeIdx(cfg, stage, 1.0, DES_INVALID_ID, 0, "PASS");
DesConfig_addArrivalIdx(cfg, "Release", 50, stage, DES_DIST_FIXED, 10, 0);
DesConfig_setSeed(cfg, 42);

DesValidationResult validation;
if (!DesConfig_validate(cfg, &validation)) {
    DesConfig_destroy(cfg);
    return 1;
}

DesEngine *engine = DesEngine_create(cfg);
DesErrorCode result = DesEngine_run(engine);
DesStats_printSummary(engine);
DesEngine_destroy(engine);
DesConfig_destroy(cfg);
```

`DesConfig_init()` (a macro for `DesConfig_create()`) and `DesConfig_create()` return a heap-allocated config with identical runnable defaults. Always call `DesConfig_destroy()` when done. `DesConfig_saveJson()` is the shared, validated, atomic JSON serializer.

## Repository structure

```text
framework/             Native simulation library
apps/desim/            Experiment and agent CLI
apps/workbench/        Dependency-free visual workbench
apps/config_runner/    Optional human-readable compatibility runner
apps/mdf_export/       MDF4 resource-utilization export
configs/               Example scenarios and JSON schema
tests/                 Offline deterministic test suite
docs/                  Architecture documentation
```

The old Windows Console TUI is retained only as deprecated source. It is not built by default. It can be enabled on Windows with `-DBUILD_LEGACY_TUI=ON`.

## Build options

| Option | Default | Purpose |
|---|---:|---|
| `DES_BUILD_APPS` | `ON` | Build native applications |
| `DES_BUILD_EXAMPLES` | `OFF` | Build legacy example runners |
| `BUILD_TESTS` | `ON` | Build the offline test suite |
| `DES_ENABLE_WARNINGS` | `ON` | Enable recommended compiler warnings |
| `BUILD_LEGACY_TUI` | `OFF` | Build the deprecated Windows-only editor |

The test suite has no downloaded dependencies and can run in offline or controlled build environments.

## Current scope

This milestone supports resources, availability dates, queues, processing distributions, deterministic replay, probabilistic routing, rework loops, visual manual FSMs, structured service objectives, and capacity sweeps. Calendars, skills, dependency gates, imported empirical distributions, and automated multi-variable optimization remain planned extensions.
