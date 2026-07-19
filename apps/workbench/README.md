# FlowLab visual workbench

Open `index.html` directly in a modern browser. The workbench has no external dependencies and stores no data outside files explicitly imported or exported by the user.

Workflow:

1. Import an existing scenario or edit the included neutral service-operation model.
2. Resolve any validation issues shown in the model view.
3. Export the scenario as `scenario.json`.
4. Use Replay to copy a deterministic `desim run --replay replay.json` command, then import the generated replay.
5. Configure a throughput or mean-flow service objective in Experiment and copy the capacity-sweep command.
6. Import the version-2 sweep JSON in Results to see the smallest confidence-qualified capacity.

Native validation and execution are performed by `desim`; the browser remains an editor, replay player, and results viewer rather than containing a second simulation engine.
