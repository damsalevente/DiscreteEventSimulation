# FlowLab architecture

## Purpose

FlowLab separates editable scenarios, validated runtime models, simulation execution, experiments, and user interfaces. A scenario has one canonical meaning whether it is created in JSON, through the C builder API, or in the visual workbench.

```text
Visual workbench ─┐
JSON / AI agent ──┼─> DesSimConfig ─> validation ─> DesEngine
C builder API ────┘                                  │
                                                     ├─> event queue
                                                     ├─> entities
                                                     ├─> resources
                                                     ├─> stage FSMs
                                                     └─> statistics

desim experiment/sweep ─> independent engines ─> structured aggregate results
```

## Layers

### Scenario model

`DesSimConfig` is the editable data-transfer model. It contains resources, stages, arrivals, limits, seed, and recording settings. Collections have explicit compile-time limits so malformed or unexpectedly large inputs can be rejected before execution.

There are two construction paths:

- `DesConfig_loadJson*()` parses the versioned JSON format.
- `DesConfig_*` and `DesStage_*` construct the same model programmatically.

`DesConfig_init()` and `DesConfig_create()` use identical defaults. `DesConfig_saveJson()` validates first and writes through a temporary file so a failed save does not truncate the destination.

### Validation

`DesConfig_validate()` is the authority for semantic model validity. It checks:

- Positive simulation limits and capacities
- Unique resource and stage names
- Resource and stage references
- Distribution types and parameters
- State, event, and FSM transition indices
- Duplicate transition keys
- Outcome probabilities and routing targets
- Arrival stage references and entity capacity

The JSON loader rejects a model when parsing or validation fails. The engine also validates before allocating runtime state, protecting callers that build models directly in C.

### Runtime engine

`DesEngine` owns mutable runtime state. The source configuration remains borrowed and must outlive the engine.

Runtime responsibilities are deliberately separated:

- `event_queue.c` — dynamically growing min-heap ordered by time, priority, and event ID
- `engine.c` — arrival seeding, event stepping, FSM dispatch, limits, and error propagation
- `entity.c` — entity lifecycle, per-entity state, entry time, completion time, and stage visits
- `resource.c` — resource instance acquisition, release, and scheduled availability
- `rng.c` — deterministic samples and probabilistic outcome selection
- `stats.c` — flow and resource records, console summaries, and CSV reports
- `des_mdf.c` — MDF4 resource-utilization export

### FSM execution

Each stage defines a transition table:

```text
(current state, event type) -> (next state, action)
```

FSM state is associated with the entity and reset to the destination stage's explicit initial state when routing. Resource occupancy is recorded separately. This allows concurrent entities and lets multiple stages share a resource pool without interpreting one another's state indices.

The engine infers a stage's entry event from an acquire/enter transition and its completion event from a release-and-dispatch transition. Manual FSMs therefore do not depend on event names or fixed event indices.

When a resource is unavailable, the entry event remains in its original state and is rescheduled. A resource with a future `available_at` time is retried directly at that time when possible, avoiding long polling sequences.

### Entity timing

Arrival streams seed lightweight arrival events, not entities. The entity is created when its arrival event is processed. Consequently, `entry_time` is the real system-arrival time and flow time does not include time before arrival.

### Limits and lifecycle

- `max_time` is inclusive: events after the configured horizon remain unprocessed.
- `max_events` limits processed events, not queue capacity.
- Internal scheduling errors propagate through `last_error` and stop execution.
- Calling `DesEngine_run()` a second time continues the same engine and never seeds duplicate entities.
- Independent replications create independent engines and use explicit seeds.

## Experiment interface

`apps/desim` is the stable automation boundary:

- `validate` emits model diagnostics.
- `run` executes one deterministic replication.
- `experiment` aggregates completed count, completion rate, throughput, flow time, makespan, and utilization across independent runs.
- `sweep` varies one resource capacity and recommends the smallest capacity whose confidence bound satisfies `throughput>=...` or `mean-flow<=...`.
- `run --replay FILE` exports deterministic stage visits, FSM transitions, and resource assignments for visual playback.

Experiment JSON uses `result_version: 2`; replay JSON uses `replay_version: 1`. Both are suitable for agents and the browser workbench. Exit codes distinguish usage, model-loading, and simulation failures.

## Visual workbench

`apps/workbench` is a dependency-free HTML, CSS, and JavaScript application. It edits simple and advanced FSM stages, renders actual outcome routes, safely removes resource pools, prepares native commands, replays native traces, and visualizes structured sweep results.

The workbench deliberately does not contain a second simulation engine. Native execution remains authoritative until a local service or WebAssembly bridge can call the same engine directly.

## Performance model

- Event scheduling: `O(log n)` through the min-heap
- Model lookup: validated numeric IDs during execution
- Runtime allocation: performed during engine creation or controlled collector/queue growth
- Replications: independent and suitable for process-level parallelism

The engine avoids JSON parsing and string lookup inside the event loop. Statistics tracing can be disabled when maximum throughput is required.

## Extension roadmap

The next model-level extensions should preserve the canonical validation boundary:

1. Resource calendars and shifts
2. Skills and eligibility constraints
3. Dependency gates and synchronization
4. Empirical and additional probability distributions
5. Batch and preemption policies
6. Parallel replication executor
7. Multi-variable search and optimization
8. A local service or WebAssembly bridge for one-click browser execution

## Deprecated surface

`apps/tui_editor` directly edits internal structures and contains a separate lossy JSON writer. It is Windows-only and excluded from default builds. It remains temporarily for reference behind `BUILD_LEGACY_TUI` and should not receive new features.
