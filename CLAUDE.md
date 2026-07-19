# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

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

## Architecture

C11 discrete-event simulation framework with JSON-driven configuration and FSM-based stage processing. Entities flow through stages, each stage may acquire/release resources and dispatch to outcomes based on probabilistic branching.

### Core files

- `framework/include/des/des_types.h` — All shared types: `Event`, `SimConfig`, `Stage`, `Entity`, FSM entries, distributions, error codes, stats types. Fixed-size arrays with compile-time limits (`DES_MAX_*`).
- `framework/include/des/des_config.h` + `framework/src/config.c` — Config builder API. String-based functions for readable config construction. `DesStage_setResourceMode()` shorthand auto-generates IDLE/BUSY FSM. `DesConfig_getLastError()` for error reporting. Convenience macros: `DesConfig_init()`, `DES_DIST_*()`.
- `framework/src/engine.c` — Simulation engine: `DesEngine_create`, `DesEngine_run`, `DesEngine_step`. Seeds entities from arrivals, dispatches actions via FSM lookup, records stats.
- `framework/src/event_queue.c` — Min-heap priority queue for events. O(log n) enqueue/dequeue.
- `framework/src/json_load.c` + `json_parser.inc` — JSON parser (vendored recursive-descent) and config loader. `DesConfig_getLoadError()` for parse error details. Supports `"mode": "resource"` shorthand to auto-generate FSM.
- `framework/src/resource.c` — Resource acquire/release with availability tracking and time-based delays.
- `framework/src/entity.c` — Entity lifecycle: create, enter stage, exit system.
- `framework/src/rng.c` — Deterministic RNG (LCG). Exponential, uniform, normal, outcome selection.
- `framework/src/stats.c` — Records entity flow transitions and resource state changes. `DesStats_printSummary()` and `DesStats_printConfigSummary()` for rich console output.
- `framework/src/des_mdf.c` — ASAM MDF4 file export for resource utilization data.

### Apps

- `apps/coffeesim/` — Simple CLI runner (loads JSON, runs sim, prints report)
- `apps/config_runner/` — Generic CLI runner with verbose output
- `apps/tui_editor/` — Windows Console API-based TUI: interactive form-based config editor with JSON save
- `apps/mdf_export/` — Runs simulation and exports resource utilization to MDF4
- `apps/release_pipeline/` — Release pipeline demo runner

### Tests

- `tests/test_event_queue.c` — Queue operations and heap ordering
- `tests/test_engine.c` — Engine lifecycle with programmatic config
- `tests/test_config.c` — Config builder (string + index APIs), JSON loading, dist macros
- `tests/test_airport.c` — Airport security simulation (JSON string + programmatic)
- `tests/test_whatif.c` — Multi-stage what-if scenario with resource delays

### Known issues

- JSON parser is vendored as a static include (`json_parser.inc`) — works but not independently testable.
- TUI apps use Windows Console API directly — not portable to non-Windows platforms.
- Pre-existing test failures: `test_airport` and `test_whatif` JSON file-loading tests fail when run from `build/tests/` due to relative path to `configs/`.

### JSON config format

Configs support two modes per stage:
- `"mode": "resource"` (recommended): auto-generates IDLE/BUSY states, ENTER/COMPLETE events, and acquire/release FSM. Just specify `resource`, `processing_time`, and `outcomes`.
- `"mode": "manual"` (default): explicit `states`, `event_types`, `fsm` arrays.

Schema: `configs/des_schema.json`
