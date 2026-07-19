# Software Architecture Document: DES Framework

## 1. Executive Summary

The DES Framework is a C11 library for discrete-event simulation. It provides a configurable simulation engine where entities (jobs, customers, vehicles) flow through stages (service points), each stage may acquire/release shared resources and dispatch entities to outcomes based on probabilistic branching. The framework supports JSON-driven configuration, deterministic seeding, and post-simulation statistics export.

## 2. Architectural Overview

```
                        ┌─────────────────────────────────────────┐
                        │            Application Layer            │
                        │  coffeesim | config_runner | tui_editor │
                        └─────────────────┬───────────────────────┘
                                          │
                        ┌─────────────────┴───────────────────────┐
                        │            Framework Layer              │
                        │  ┌──────────┐ ┌──────────┐ ┌─────────┐ │
                        │  │  Config   │ │  Engine  │ │  Stats  │ │
                        │  │  Builder  │ │  (FSM)   │ │  & CSV  │ │
                        │  └──────────┘ └──────────┘ └─────────┘ │
                        │  ┌──────────┐ ┌──────────┐ ┌─────────┐ │
                        │  │  Event   │ │ Resource │ │  Entity │ │
                        │  │  Queue   │ │  Mgmt    │ │  Mgmt   │ │
                        │  └──────────┘ └──────────┘ └─────────┘ │
                        │  ┌──────────┐ ┌──────────┐             │
                        │  │   RNG    │ │JSON Load │             │
                        │  │(distrib.)│ │  (cJSON) │             │
                        │  └──────────┘ └──────────┘             │
                        └─────────────────────────────────────────┘
```

## 3. Component Architecture

### 3.1 Event Queue (`event_queue.c`)

**Purpose:** Manages the lifecycle of simulation events using a min-heap priority queue.

**Data Structure:** Heap-allocated buffer with O(log n) enqueue/dequeue. Events are sorted by `(time, priority, id)`.

**Key types:**
- `DesEvent` — id, target_stage_id, event_type, entity_id, time, priority, data[4]

**API:**
- `DesEventQueue_init/destroy/reset`
- `DesEventQueue_enqueue/dequeue/peek`
- `DesEventQueue_isEmpty/isFull/size`

**Note:** `DesEventQueue_sort()` is a no-op since the heap already maintains order. It remains in the API for backward compatibility but is called unnecessarily in `DesEngine_step()`.

### 3.2 Simulation Engine (`engine.c`)

**Purpose:** Orchestrates the simulation lifecycle. Creates resources, stages, entities, and processes events through FSM dispatch.

**Execution model:**
1. `DesEngine_create()` — Allocate and initialize from `DesSimConfig`
2. `DesEngine_run()` — Seed arrival events, then loop `DesEngine_step()` until empty/max_time
3. `DesEngine_step()` — Dequeue next event, lookup FSM entry for `(current_state, event_type)`, dispatch action, update state

**FSM Dispatch:**
Each stage has a 2D FSM table: `fsm[state * num_event_types + event_type]`. Each entry contains `(next_state, action_type, custom_action_id)`. The engine's `dispatchAction()` handles built-in action types:
- `ACQUIRE_AND_PROCESS` — Acquire resource, schedule completion event after processing delay
- `RELEASE_AND_DISPATCH` — Release resource, select outcome, dispatch to next stage or exit
- `RELEASE_AND_RETRY` — Release and reschedule with delay
- `WAIT_RETRY` — Reschedule without releasing
- `NONE` — No action

**Resource contention:** When `acquire_and_process` fails (no available instances), the event is rescheduled with a calculated delay based on the earliest `available_at_time` of unassigned instances.

### 3.3 Configuration Builder (`config.c`, `des_config.h`)

**Purpose:** Two modes of configuration — JSON file loading and programmatic C API.

**JSON Loading (`json_load.c`):**
- Uses a vendored recursive-descent JSON parser (`json_parser.inc`)
- Reads into `DesSimConfig` struct via `DesConfig_loadJson()` / `DesConfig_loadJsonString()`
- Supports `"mode": "resource"` shorthand that auto-generates IDLE/BUSY states, ENTER/COMPLETE events, and acquire/release FSM
- Supports `entity_capacity` field (auto-calculates from resources if omitted)
- Proper boolean parsing via `safeBool()` (checks `cJSON_True`/`cJSON_False` type, not just key presence)
- Error details via `DesConfig_getLoadError()` (returns static buffer with parse failure context)

**Programmatic API:**
Two parallel APIs:

1. **String-based** (recommended for readable code):
   - `DesStage_addTransition(cfg, stage, "IDLE", "ENTER", "BUSY", action)`
   - `DesStage_addOutcome(cfg, stage, 0.7, "NextStage", "ENTER", "PASS")`
   - `DesConfig_addArrival(cfg, name, count, "EntryStage", "ENTER", dist, p1, p2)`

2. **Index-based** (for performance or dynamic generation):
   - `DesStage_addTransitionIdx(cfg, stage, from_state_idx, event_idx, to_state_idx, action)`
   - `DesStage_addOutcomeIdx(cfg, stage, prob, next_stage_id, next_event_idx, name)`
   - `DesConfig_addArrivalIdx(cfg, name, count, entry_stage_id, dist, p1, p2)`

**Resource mode shorthand:**
- `DesStage_setResourceMode(cfg, stage_id, resource_id)` — Auto-generates IDLE/BUSY states, ENTER/COMPLETE events, and the standard acquire/process/release FSM. Must be called after all primitive additions (states, events, transitions, outcomes) since it references them by index.

**Stack allocation:** `DesConfig_init()` returns a zero-initialized `DesSimConfig` on the stack (no malloc). For heap allocation, use `DesConfig_create()`/`DesConfig_destroy()`.

**Error reporting:**
- `DesConfig_getLastError(cfg)` — Returns last builder error string (NULL if no error)
- `DesConfig_getLoadError()` — Returns static buffer with JSON parse/load error details
- `DesError_toString(code)` — Maps `DesErrorCode` enum to human-readable string

**Convenience macros:**
- `DesConfig_init()` — Zero-initialize a `DesSimConfig`
- `DES_DIST_FIX(v, out)` — Fixed distribution
- `DES_DIST_EXP(lam, out)` — Exponential distribution
- `DES_DIST_UNI(a, b, out)` — Uniform distribution
- `DES_DIST_NORM(m, s, out)` — Normal distribution
- Legacy aliases: `DES_DIST_FMT`, `DES_DIST_NRM` (same signatures)

### 3.4 Resource Management (`resource.c`)

**Purpose:** Manages resource instances with availability tracking and time-based delays.

**Model:**
- Each resource type has N instances
- Each instance tracks: assigned entity, assigned stage, current state, available_at_time
- `available_at_time` supports resources that become available at a specific simulation time (e.g., a vehicle arriving later)

**API:**
- `DesResource_acquire()` — Returns instance_id or -1 if none available
- `DesResource_release()` — Marks instance as unassigned
- `DesResource_getAvailable()` — Returns count of available instances

### 3.5 Entity Management (`entity.c`)

**Purpose:** Tracks entity lifecycle through the simulation.

**Entity states:**
- `active = true` — Currently in the system
- `active = false, completion_time > 0` — Successfully completed
- `current_stage_id = DES_INVALID_ID` — Exited the system

**Tracking:**
- `entry_time` — When the entity first entered the system
- `stage_entry_time` — When the entity entered the current stage
- `num_stage_visits` — Total stages visited (for pipeline analysis)
- `outcome_id` — Final outcome (-1 if still active)

### 3.6 Random Number Generation (`rng.c`)

**Purpose:** Deterministic RNG using a Linear Congruential Generator (LCG).

**Formula:** `state = state * 1103515245 + 12345`

**Distributions:**
- `DES_DIST_FIXED` — Constant value
- `DES_DIST_UNIFORM` — Integer uniform [min, max]
- `DES_DIST_EXPONENTIAL` — `floor(-log(1-u) / lambda)` where u is uniform (0,1)
- `DES_DIST_NORMAL` — Box-Muller transform

**Outcome selection:** Cumulative probability distribution with deterministic roll.

### 3.7 Statistics & Reporting (`stats.c`)

**Purpose:** Records entity flow transitions and resource state changes. Produces rich console output and optional CSV export.

**Recorded data:**
- `DesEntityRecord` — entity_id, stage_id, enter_time, exit_time, outcome_id
- `DesResourceRecord` — time, resource_type_id, instance_id, state, entity_id

**Console output (`DesStats_printSummary()`):**
- Per-stage breakdown: entity counts, avg/min/max time, throughput, resource utilization
- Per-resource utilization: available/busy/queue percentages
- Per-outcome distribution: percentage of entities at each exit point
- Pipeline diagram: visual flow with avg time per stage
- Flow time histogram (8 buckets)
- Entity arrival/departure timeline

**Console config summary (`DesStats_printConfigSummary()`):**
- Resources, stages, FSM transitions, outcomes, arrival streams

**CSV export:**
- `entity_flow.csv` — Per-entity stage transit times
- `resource_util.csv` — Resource state changes over time

### 3.8 MDF4 Export (`des_mdf.c`)

**Purpose:** Exports resource utilization data to ASAM MDF4.10 format for analysis in tools like CANape or MDF validators.

## 4. Data Flow

```
JSON Config File
      │
      ▼
DesConfig_loadJson() ──► DesSimConfig struct
      │
      ▼
DesEngine_create() ──► DesEngine (runtime state)
      │
      ├──► seedEntities() ──► arrival events in queue
      │
      ▼
DesEngine_run() loop:
      │
      ├──► DesEngine_step()
      │      │
      │      ├──► DesEventQueue_dequeue()
      │      │
      │      ├──► FSM lookup: stages[stage_id].fsm[state * event_types + event_type]
      │      │
      │      ├──► dispatchAction()
      │      │      ├── acquire/release resources
      │      │      ├── schedule new events
      │      │      └── record stats
      │      │
      │      └──► update FSM state
      │
      └──► DesStats_generateReport() ──► CSV files
```

## 5. Key Design Decisions

### 5.1 Fixed-Size Arrays
All collections use fixed-size arrays with compile-time limits (`DES_MAX_*`). This ensures deterministic memory usage and avoids allocation failures during simulation. Limits are generous (100k events, 10k entities, 64 resources/stages) but can be adjusted in `des_types.h`.

### 5.2 Deterministic Simulation
The LCG-based RNG with explicit seeding ensures reproducible results. Two simulations with the same seed produce identical event sequences and statistics.

### 5.3 FSM-Driven Processing
Each stage is a finite state machine with states (IDLE, BUSY, custom), event types (ENTER, DONE, custom), and actions. This allows modeling complex workflows beyond simple acquire-process-release.

### 5.4 JSON as Primary Config Format
JSON provides human-readable, tool-editable configuration. The C API exists for programmatic use and testing, but JSON is the recommended approach for production configs.

### 5.5 Separation of Config and Engine
`DesSimConfig` is immutable after construction. `DesEngine` owns all mutable runtime state. This separation allows running the same config multiple times with different seeds.

## 6. Limitations & Known Issues

1. **No dynamic resizing** — Fixed arrays mean configs exceeding `DES_MAX_*` limits cannot be loaded. The limits are compile-time constants.
2. **Single-threaded** — The engine is single-threaded. Parallel simulation would require partitioning the event queue.
3. **Vendored JSON parser** — `json_parser.inc` is a static include that cannot be tested independently.
4. **Platform-specific TUI** — TUI apps use the Windows Console API directly and are not portable to non-Windows platforms without modification.
5. **Resource time-based availability** — `available_at_time` is per-instance and checked during acquire, but released instances don't reset this field.
6. **Pre-existing test path issues** — `test_airport` and `test_whatif` JSON file-loading subtests fail when run from `build/tests/` due to relative path to `configs/`.

## 7. Extension Points

### 7.1 Custom Actions
Register custom action handlers via `DesEngine_registerAction()`. These are stored in the engine's `custom_actions` array but are not currently dispatched by the built-in FSM (the engine handles all built-in actions inline). To use custom actions, modify `dispatchAction()` in `engine.c`.

### 7.2 New Distributions
Add new `DesDistType` values in `des_types.h` and implement sampling in `DesRng_sample()`.

### 7.3 New Output Formats
The stats collector stores raw records that can be exported to any format. See `des_mdf.c` for an example of a non-CSV exporter.

### 7.4 New App Types
Link against `des_framework` and use the `DesConfig`/`DesEngine`/`DesStats` APIs. See `apps/coffeesim/` for the minimal example.
