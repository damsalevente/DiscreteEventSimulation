const defaultScenario = {
  format_version: 1,
  name: "Service operation",
  simulation: { max_time: 10000, max_events: 100000, entity_capacity: 0, seed: 42 },
  resources: [
    { name: "Service_Agent", count: 2, available_at: 0 },
    { name: "Reviewer", count: 1, available_at: 0 }
  ],
  stages: [
    { name: "Intake", mode: "resource", resource: "Service_Agent", processing_time: { distribution: "fixed", param1: 6, param2: 0 }, outcomes: [{ name: "READY", probability: 1, next_stage: "Fulfillment", next_event: "ENTER" }] },
    { name: "Fulfillment", mode: "resource", resource: "Service_Agent", processing_time: { distribution: "normal", param1: 18, param2: 4 }, outcomes: [{ name: "REVIEW", probability: .9, next_stage: "Review", next_event: "ENTER" }, { name: "CLOSED", probability: .1, next_stage: null }] },
    { name: "Review", mode: "resource", resource: "Reviewer", processing_time: { distribution: "fixed", param1: 8, param2: 0 }, outcomes: [{ name: "APPROVED", probability: .85, next_stage: null }, { name: "REWORK", probability: .15, next_stage: "Fulfillment", next_event: "ENTER" }] }
  ],
  entity_arrivals: [{ name: "Requests", count: 80, entry_stage: "Intake", inter_arrival: { distribution: "exponential", param1: .12, param2: 0 }, start_time: 0, priority: 0 }],
  statistics: { record_events: true, record_entity_flow: true, record_resource_util: true, output_dir: "./output/service_operation" }
};

const actionOptions = [
  ["acquire_and_process", "Start processing"],
  ["release_and_dispatch", "Complete and route"],
  ["release_and_retry", "Release and retry"],
  ["wait_retry", "Wait and retry"],
  ["entity_enter", "Enter stage"],
  ["entity_exit", "Exit system"],
  ["none", "No action"]
];

let scenario = structuredClone(defaultScenario);
let selectedStage = 0;
let replayData = null;
let replayTime = 0;
let replayPlaying = false;
let replayFrame = null;
let replayLastFrame = 0;
let replaySelectedStage = 0;
let replayEventTimes = [];
let replayEntityStates = new Map();
let replayAppliedTransition = 0;
let replayAppliedTime = -1;
let replayTimelineCache = new Map();
let replayAssignmentStages = new Map();
let replayTransitionsByTime = new Map();
const $ = selector => document.querySelector(selector);
const $$ = selector => [...document.querySelectorAll(selector)];

function escapeHtml(value) {
  return String(value ?? "").replace(/[&<>'"]/g, char => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", "'": "&#39;", '"': "&quot;" })[char]);
}

function toast(message) {
  const element = $("#toast");
  element.textContent = message;
  element.classList.add("visible");
  setTimeout(() => element.classList.remove("visible"), 1800);
}

function normalizeScenario(value) {
  return {
    format_version: 1,
    name: value.name || "Imported scenario",
    simulation: { max_time: 100000, max_events: 100000, entity_capacity: 0, seed: 42, ...(value.simulation || {}) },
    resources: Array.isArray(value.resources) ? value.resources : [],
    stages: Array.isArray(value.stages) ? value.stages : [],
    entity_arrivals: Array.isArray(value.entity_arrivals) ? value.entity_arrivals : [],
    statistics: { record_events: true, record_entity_flow: true, record_resource_util: true, output_dir: "./output", ...(value.statistics || {}) }
  };
}

function manualStage(stage) {
  return (stage?.mode || "manual") === "manual";
}

function makeManual(stage) {
  stage.mode = "manual";
  stage.states = ["IDLE", "BUSY"];
  stage.initial_state = "IDLE";
  stage.event_types = ["ENTER", "COMPLETE"];
  stage.fsm = [
    { state: "IDLE", event: "ENTER", next_state: "BUSY", action: "acquire_and_process" },
    { state: "BUSY", event: "COMPLETE", next_state: "IDLE", action: "release_and_dispatch" }
  ];
}

function makeSimple(stage) {
  stage.mode = "resource";
  delete stage.states;
  delete stage.initial_state;
  delete stage.event_types;
  delete stage.fsm;
}

function stageEntryEvent(stage) {
  if (!stage || !manualStage(stage)) return "ENTER";
  const transition = (stage.fsm || []).find(item => item.action === "acquire_and_process" || item.action === "entity_enter");
  return transition?.event || stage.event_types?.[0] || "ENTER";
}

function validate() {
  const errors = [];
  const resourceNames = new Set();
  scenario.resources.forEach(resource => {
    if (!resource.name || resourceNames.has(resource.name)) errors.push("Resource names must be unique and non-empty.");
    resourceNames.add(resource.name);
    if (!(Number(resource.count) > 0)) errors.push(`${resource.name || "Resource"} needs positive capacity.`);
  });
  const stageNames = new Set();
  scenario.stages.forEach(stage => {
    if (!stage.name || stageNames.has(stage.name)) errors.push("Stage names must be unique and non-empty.");
    stageNames.add(stage.name);
  });
  scenario.stages.forEach(stage => {
    if (stage.resource && !resourceNames.has(stage.resource)) errors.push(`${stage.name} uses an unknown resource.`);
    const sum = (stage.outcomes || []).reduce((total, outcome) => total + Number(outcome.probability || 0), 0);
    if (stage.outcomes?.length && Math.abs(sum - 1) > .000001) errors.push(`${stage.name} transition probabilities sum to ${sum.toFixed(3)}.`);
    (stage.outcomes || []).forEach(outcome => {
      if (outcome.next_stage && !stageNames.has(outcome.next_stage)) errors.push(`${stage.name} links to an unknown stage.`);
      if (outcome.next_stage) {
        const target = scenario.stages.find(item => item.name === outcome.next_stage);
        const events = manualStage(target) ? (target?.event_types || []) : ["ENTER"];
        if (!events.includes(outcome.next_event || stageEntryEvent(target))) errors.push(`${stage.name} routes to an unknown event on ${outcome.next_stage}.`);
      }
    });
    if (manualStage(stage)) {
      const states = stage.states || [];
      const events = stage.event_types || [];
      if (!states.length || new Set(states).size !== states.length || states.some(name => !name)) errors.push(`${stage.name} needs unique, named FSM states.`);
      if (!events.length || new Set(events).size !== events.length || events.some(name => !name)) errors.push(`${stage.name} needs unique, named FSM events.`);
      if (!states.includes(stage.initial_state || states[0])) errors.push(`${stage.name} has an invalid initial state.`);
      const pairs = new Set();
      (stage.fsm || []).forEach(transition => {
        if (!states.includes(transition.state) || !states.includes(transition.next_state) || !events.includes(transition.event)) errors.push(`${stage.name} has an FSM transition with an unknown state or event.`);
        if (!actionOptions.some(([value]) => value === transition.action)) errors.push(`${stage.name} uses an unsupported browser action.`);
        const pair = `${transition.state}\u0000${transition.event}`;
        if (pairs.has(pair)) errors.push(`${stage.name} has duplicate transitions for ${transition.state}/${transition.event}.`);
        pairs.add(pair);
      });
    }
  });
  scenario.entity_arrivals.forEach(arrival => { if (!stageNames.has(arrival.entry_stage)) errors.push(`${arrival.name} has an unknown entry stage.`); });
  const pill = $("#validation-pill");
  pill.textContent = errors.length ? `${errors.length} issue${errors.length === 1 ? "" : "s"}` : "Valid";
  pill.className = `status ${errors.length ? "invalid" : "valid"}`;
  const banner = $("#validation-banner");
  banner.hidden = !errors.length;
  banner.textContent = errors[0] || "";
  return errors;
}

function renderResources() {
  $("#resource-list").innerHTML = scenario.resources.map((resource, index) => `
    <div class="resource-row"><div><strong>${escapeHtml(resource.name)}</strong><small>Available at ${Number(resource.available_at || 0).toLocaleString()}</small></div>
    <input data-resource-count="${index}" type="number" min="1" value="${Number(resource.count || 1)}" aria-label="${escapeHtml(resource.name)} capacity">
    <button data-remove-resource="${index}" class="remove-button" aria-label="Remove ${escapeHtml(resource.name)}">×</button></div>`).join("") || `<p class="empty-state">No constrained resources.</p>`;
  $("#arrival-list").innerHTML = scenario.entity_arrivals.map(arrival => `
    <div class="arrival-row"><strong>${escapeHtml(arrival.name)}</strong><small>${Number(arrival.count || 0)} entities → ${escapeHtml(arrival.entry_stage || "unassigned")}</small></div>`).join("") || `<p class="empty-state">No arrival streams.</p>`;
  $("#sweep-resource").innerHTML = scenario.resources.map(resource => `<option value="${escapeHtml(resource.name)}">${escapeHtml(resource.name)}</option>`).join("");
  $$('[data-resource-count]').forEach(input => input.addEventListener("change", event => {
    scenario.resources[Number(event.target.dataset.resourceCount)].count = Math.max(1, Number(event.target.value));
    updateCommand(); validate();
  }));
  $$('[data-remove-resource]').forEach(button => button.addEventListener("click", event => {
    const index = Number(event.currentTarget.dataset.removeResource);
    const resource = scenario.resources[index];
    const users = scenario.stages.filter(stage => stage.resource === resource.name).map(stage => stage.name);
    if (users.length && !window.confirm(`Remove ${resource.name}? These stages will become unconstrained: ${users.join(", ")}.`)) return;
    scenario.stages.forEach(stage => { if (stage.resource === resource.name) delete stage.resource; });
    scenario.resources.splice(index, 1);
    renderAll();
    toast(`${resource.name} removed`);
  }));
}

function renderCanvas() {
  const exitNeeded = scenario.stages.some(stage => (stage.outcomes || []).some(outcome => !outcome.next_stage));
  const width = Math.max(650, scenario.stages.length * 260 + (exitNeeded ? 170 : 40));
  $("#process-canvas").innerHTML = `<div class="flow-board" style="width:${width}px"><svg class="flow-links" aria-hidden="true"><defs><marker id="flow-arrow" markerWidth="8" markerHeight="8" refX="7" refY="4" orient="auto"><path d="M0,0 L8,4 L0,8 z"></path></marker></defs><g></g></svg><div class="flow-nodes">${scenario.stages.map((stage, index) => {
    const distribution = stage.processing_time || { distribution: "fixed", param1: 0 };
    return `<button class="stage-node ${index === selectedStage ? "selected" : ""}" data-stage="${index}">
      <span class="node-type">${manualStage(stage) ? "advanced FSM" : "simple"}</span><h3>${escapeHtml(stage.name)}</h3>
      <p>${escapeHtml(stage.resource || "Unconstrained")}</p>
      <div class="node-metrics"><span>Duration<strong>${escapeHtml(distribution.distribution)} ${Number(distribution.param1 || 0)}</strong></span><span>Routes<strong>${(stage.outcomes || []).length}</strong></span></div>
    </button>`;
  }).join("")}${exitNeeded ? `<div id="flow-exit" class="flow-exit"><span>Exit</span><strong>System</strong></div>` : ""}</div></div>`;
  $$('[data-stage]').forEach(node => node.addEventListener("click", event => {
    selectedStage = Number(event.currentTarget.dataset.stage); renderCanvas(); renderInspector();
  }));
  requestAnimationFrame(drawCanvasLinks);
}

function drawCanvasLinks() {
  const board = $(".flow-board");
  if (!board) return;
  const svg = board.querySelector(".flow-links");
  const group = svg.querySelector("g");
  const nodes = [...board.querySelectorAll(".stage-node")];
  const exit = board.querySelector("#flow-exit");
  svg.setAttribute("viewBox", `0 0 ${board.clientWidth} ${board.clientHeight}`);
  const paths = [];
  scenario.stages.forEach((stage, sourceIndex) => (stage.outcomes || []).forEach((outcome, outcomeIndex) => {
    const source = nodes[sourceIndex];
    const targetIndex = scenario.stages.findIndex(item => item.name === outcome.next_stage);
    const target = targetIndex >= 0 ? nodes[targetIndex] : exit;
    if (!source || !target) return;
    const sx = source.offsetLeft + source.offsetWidth;
    const sy = source.offsetTop + source.offsetHeight / 2 + (outcomeIndex - ((stage.outcomes || []).length - 1) / 2) * 12;
    const tx = target.offsetLeft;
    const ty = target.offsetTop + target.offsetHeight / 2;
    const backward = tx <= sx;
    const bend = backward ? Math.max(source.offsetTop + source.offsetHeight, target.offsetTop + target.offsetHeight) + 70 + outcomeIndex * 18 : (sx + tx) / 2;
    const d = backward ? `M${sx},${sy} C${sx + 38},${bend} ${tx - 38},${bend} ${tx},${ty}` : `M${sx},${sy} C${bend},${sy} ${bend},${ty} ${tx},${ty}`;
    paths.push(`<path class="route-line" d="${d}" marker-end="url(#flow-arrow)"><title>${escapeHtml(stage.name)} — ${escapeHtml(outcome.name)} → ${escapeHtml(outcome.next_stage || "Exit")}</title></path>`);
  }));
  group.innerHTML = paths.join("");
}

function renameState(stage, oldName, newName) {
  stage.states = stage.states.map(name => name === oldName ? newName : name);
  if (stage.initial_state === oldName) stage.initial_state = newName;
  (stage.fsm || []).forEach(transition => {
    if (transition.state === oldName) transition.state = newName;
    if (transition.next_state === oldName) transition.next_state = newName;
  });
}

function renameEvent(stage, oldName, newName) {
  stage.event_types = stage.event_types.map(name => name === oldName ? newName : name);
  (stage.fsm || []).forEach(transition => { if (transition.event === oldName) transition.event = newName; });
  scenario.stages.forEach(source => (source.outcomes || []).forEach(outcome => {
    if (outcome.next_stage === stage.name && outcome.next_event === oldName) outcome.next_event = newName;
  }));
}

function renderFsmPreview(stage) {
  const transitions = stage.fsm || [];
  return `<div class="fsm-preview"><div class="fsm-state-row">${(stage.states || []).map(state => `<span class="fsm-state ${state === (stage.initial_state || stage.states?.[0]) ? "initial" : ""}">${escapeHtml(state)}</span>`).join("")}</div><div class="fsm-arrow-list">${transitions.map(transition => `<span><strong>${escapeHtml(transition.state)}</strong> —${escapeHtml(transition.event)}→ <strong>${escapeHtml(transition.next_state)}</strong></span>`).join("") || `<span>No transitions yet.</span>`}</div></div>`;
}

function renderAdvancedEditor(stage) {
  const states = stage.states || [];
  const events = stage.event_types || [];
  return `<div class="advanced-editor">
    <div class="section-heading"><h2>States</h2><button id="add-state" class="icon-button" aria-label="Add state">+</button></div>
    ${states.map((state, index) => `<div class="name-row"><input data-state-name="${index}" value="${escapeHtml(state)}"><button data-remove-state="${index}" class="remove-button">×</button></div>`).join("")}
    <label class="field"><span>Initial state</span><select id="initial-state">${states.map(state => `<option ${state === (stage.initial_state || states[0]) ? "selected" : ""}>${escapeHtml(state)}</option>`).join("")}</select></label>
    <div class="section-heading"><h2>Events</h2><button id="add-event" class="icon-button" aria-label="Add event">+</button></div>
    ${events.map((eventName, index) => `<div class="name-row"><input data-event-name="${index}" value="${escapeHtml(eventName)}"><button data-remove-event="${index}" class="remove-button">×</button></div>`).join("")}
    <div class="section-heading"><h2>FSM transitions</h2></div>
    ${(stage.fsm || []).map((transition, index) => `<div class="fsm-transition-row">
      <select data-fsm-from="${index}">${states.map(state => `<option ${state === transition.state ? "selected" : ""}>${escapeHtml(state)}</option>`).join("")}</select>
      <select data-fsm-event="${index}">${events.map(name => `<option ${name === transition.event ? "selected" : ""}>${escapeHtml(name)}</option>`).join("")}</select>
      <select data-fsm-to="${index}">${states.map(state => `<option ${state === transition.next_state ? "selected" : ""}>${escapeHtml(state)}</option>`).join("")}</select>
      <select data-fsm-action="${index}">${actionOptions.map(([value, label]) => `<option value="${value}" ${value === transition.action ? "selected" : ""}>${label}</option>`).join("")}</select>
      <button data-remove-fsm="${index}" class="remove-button">×</button></div>`).join("")}
    <button id="add-fsm" class="button quiet wide">+ Add FSM transition</button>
    <div class="section-heading"><h2>State diagram</h2></div>${renderFsmPreview(stage)}
  </div>`;
}

function renderInspector() {
  const stage = scenario.stages[selectedStage];
  if (!stage) { $("#stage-title").textContent = "No stage selected"; $("#stage-form").innerHTML = ""; return; }
  $("#stage-title").textContent = stage.name;
  const distribution = stage.processing_time || (stage.processing_time = { distribution: "fixed", param1: 1, param2: 0 });
  $("#stage-form").innerHTML = `
    <label class="field"><span>Stage name</span><input id="stage-name" value="${escapeHtml(stage.name)}"></label>
    <label class="field"><span>Behavior</span><select id="stage-mode"><option value="resource" ${!manualStage(stage) ? "selected" : ""}>Simple processing</option><option value="manual" ${manualStage(stage) ? "selected" : ""}>Advanced FSM</option></select></label>
    <label class="field"><span>Resource pool</span><select id="stage-resource"><option value="">None (unconstrained)</option>${scenario.resources.map(resource => `<option value="${escapeHtml(resource.name)}" ${resource.name === stage.resource ? "selected" : ""}>${escapeHtml(resource.name)}</option>`).join("")}</select></label>
    <div class="field-pair"><label class="field"><span>Time distribution</span><select id="stage-distribution">${["fixed","uniform","exponential","normal"].map(name => `<option ${name === distribution.distribution ? "selected" : ""}>${name}</option>`).join("")}</select></label>
    <label class="field"><span>Primary value</span><input id="stage-param1" type="number" min="0" step="any" value="${Number(distribution.param1 || 0)}"></label></div>
    ${manualStage(stage) ? renderAdvancedEditor(stage) : `<p class="mode-hint">Simple mode generates an IDLE → BUSY → IDLE state machine. Choose Advanced FSM to control states, events, and actions.</p>`}
    <div class="section-heading"><h2>Probabilistic routes</h2></div>
    ${(stage.outcomes || []).map((outcome, index) => {
      const target = scenario.stages.find(item => item.name === outcome.next_stage);
      const targetEvents = target ? (manualStage(target) ? (target.event_types || []) : ["ENTER"]) : [];
      return `<div class="outcome-row"><input data-outcome-name="${index}" value="${escapeHtml(outcome.name)}" aria-label="Outcome name"><input data-outcome-probability="${index}" type="number" min="0" max="1" step="0.01" value="${Number(outcome.probability)}" aria-label="Outcome probability"><select data-outcome-target="${index}" aria-label="Next stage"><option value="">Exit system</option>${scenario.stages.map(item => `<option value="${escapeHtml(item.name)}" ${item.name === outcome.next_stage ? "selected" : ""}>${escapeHtml(item.name)}</option>`).join("")}</select><select data-outcome-event="${index}" aria-label="Next event" ${target ? "" : "disabled"}>${targetEvents.map(name => `<option ${name === (outcome.next_event || stageEntryEvent(target)) ? "selected" : ""}>${escapeHtml(name)}</option>`).join("") || `<option>—</option>`}</select></div>`;
    }).join("")}
    <button id="add-outcome" class="button quiet wide">+ Add route</button>
    <button id="remove-stage" class="button quiet danger wide">Remove stage</button>`;

  $("#stage-name").addEventListener("change", event => {
    const old = stage.name; stage.name = event.target.value.trim();
    scenario.stages.forEach(item => (item.outcomes || []).forEach(outcome => { if (outcome.next_stage === old) outcome.next_stage = stage.name; }));
    scenario.entity_arrivals.forEach(arrival => { if (arrival.entry_stage === old) arrival.entry_stage = stage.name; }); renderAll();
  });
  $("#stage-mode").addEventListener("change", event => {
    if (event.target.value === "manual") makeManual(stage);
    else if (!manualStage(stage) || window.confirm("Return to generated simple behavior? Custom FSM states and transitions will be removed.")) makeSimple(stage);
    renderAll();
  });
  $("#stage-resource").addEventListener("change", event => { stage.resource = event.target.value || undefined; renderCanvas(); validate(); });
  $("#stage-distribution").addEventListener("change", event => { distribution.distribution = event.target.value; renderCanvas(); validate(); });
  $("#stage-param1").addEventListener("change", event => { distribution.param1 = Number(event.target.value); renderCanvas(); validate(); });
  $$('[data-outcome-name]').forEach(input => input.addEventListener("change", event => { stage.outcomes[Number(event.target.dataset.outcomeName)].name = event.target.value; renderCanvas(); }));
  $$('[data-outcome-probability]').forEach(input => input.addEventListener("change", event => { stage.outcomes[Number(event.target.dataset.outcomeProbability)].probability = Number(event.target.value); validate(); }));
  $$('[data-outcome-target]').forEach(input => input.addEventListener("change", event => {
    const outcome = stage.outcomes[Number(event.target.dataset.outcomeTarget)]; outcome.next_stage = event.target.value || null;
    const target = scenario.stages.find(item => item.name === event.target.value);
    outcome.next_event = target ? stageEntryEvent(target) : undefined;
    renderCanvas(); renderInspector(); validate();
  }));
  $$('[data-outcome-event]').forEach(input => input.addEventListener("change", event => { stage.outcomes[Number(event.target.dataset.outcomeEvent)].next_event = event.target.value; validate(); }));
  $("#add-outcome").addEventListener("click", () => { stage.outcomes ||= []; stage.outcomes.push({ name: `ROUTE_${stage.outcomes.length + 1}`, probability: 0, next_stage: null }); renderInspector(); validate(); });
  $("#remove-stage").addEventListener("click", () => { if (scenario.stages.length < 2) return toast("A scenario needs at least one stage"); scenario.stages.splice(selectedStage, 1); selectedStage = Math.max(0, selectedStage - 1); renderAll(); });

  if (manualStage(stage)) bindAdvancedEditor(stage);
}

function bindAdvancedEditor(stage) {
  $("#initial-state").addEventListener("change", event => { stage.initial_state = event.target.value; validate(); renderInspector(); });
  $("#add-state").addEventListener("click", () => { let index = stage.states.length + 1; let name = `STATE_${index}`; while (stage.states.includes(name)) name = `STATE_${++index}`; stage.states.push(name); renderInspector(); validate(); });
  $$('[data-state-name]').forEach(input => input.addEventListener("change", event => { const index = Number(event.target.dataset.stateName); renameState(stage, stage.states[index], event.target.value.trim()); renderInspector(); validate(); }));
  $$('[data-remove-state]').forEach(button => button.addEventListener("click", event => { if (stage.states.length <= 1) return toast("An FSM needs one state"); const name = stage.states[Number(event.currentTarget.dataset.removeState)]; stage.states = stage.states.filter(item => item !== name); stage.fsm = (stage.fsm || []).filter(item => item.state !== name && item.next_state !== name); if (stage.initial_state === name) stage.initial_state = stage.states[0]; renderInspector(); validate(); }));
  $("#add-event").addEventListener("click", () => { let index = stage.event_types.length + 1; let name = `EVENT_${index}`; while (stage.event_types.includes(name)) name = `EVENT_${++index}`; stage.event_types.push(name); renderInspector(); validate(); });
  $$('[data-event-name]').forEach(input => input.addEventListener("change", event => { const index = Number(event.target.dataset.eventName); renameEvent(stage, stage.event_types[index], event.target.value.trim()); renderInspector(); validate(); }));
  $$('[data-remove-event]').forEach(button => button.addEventListener("click", event => { if (stage.event_types.length <= 1) return toast("An FSM needs one event"); const name = stage.event_types[Number(event.currentTarget.dataset.removeEvent)]; stage.event_types = stage.event_types.filter(item => item !== name); stage.fsm = (stage.fsm || []).filter(item => item.event !== name); renderInspector(); validate(); }));
  $("#add-fsm").addEventListener("click", () => { stage.fsm ||= []; stage.fsm.push({ state: stage.states[0], event: stage.event_types[0], next_state: stage.states[0], action: "none" }); renderInspector(); validate(); });
  ["from", "event", "to", "action"].forEach(field => $$(`[data-fsm-${field}]`).forEach(select => select.addEventListener("change", event => {
    const index = Number(event.target.dataset[`fsm${field[0].toUpperCase()}${field.slice(1)}`]);
    const key = field === "from" ? "state" : field === "to" ? "next_state" : field;
    stage.fsm[index][key] = event.target.value; renderInspector(); validate();
  })));
  $$('[data-remove-fsm]').forEach(button => button.addEventListener("click", event => { stage.fsm.splice(Number(event.currentTarget.dataset.removeFsm), 1); renderInspector(); validate(); }));
}

function objectiveExpression() {
  const metric = $("#objective-metric")?.value || "mean-flow";
  const operator = metric === "throughput" ? ">=" : "<=";
  return `${metric}${operator}${Number($("#objective-target")?.value || 0)}`;
}

function updateCommand() {
  const resource = $("#sweep-resource")?.value || scenario.resources[0]?.name || "Resource";
  const min = Number($("#sweep-min")?.value || 1);
  const max = Number($("#sweep-max")?.value || 5);
  const runs = Number($("#runs")?.value || 30);
  const seed = Number($("#seed")?.value || 42);
  $("#command-preview").textContent = `./build/apps/desim/desim sweep scenario.json --resource ${resource}=${min}:${max} --runs ${runs} --seed ${seed} --objective '${objectiveExpression()}' --json`;
  $("#replay-command").textContent = `./build/apps/desim/desim run scenario.json --seed ${seed} --replay replay.json --json`;
}

function renderResults(data) {
  const results = data?.results || [];
  if (data?.result_version !== 2 || !results.length || !data.objective) throw new Error("Unsupported results");
  const key = data.objective.metric === "throughput" ? "throughput" : "mean_flow";
  const recommended = results.find(item => item.count === data.recommended_capacity);
  const display = recommended || results[results.length - 1];
  const metric = display.summary.metrics[key];
  $("#objective-result").textContent = metric.mean.toFixed(data.objective.metric === "throughput" ? 3 : 1);
  $("#objective-confidence").textContent = `±${metric.ci95.toFixed(2)}; target ${data.objective.operator} ${data.objective.target}`;
  $("#best-capacity").textContent = data.recommended_capacity ?? "None";
  $("#best-resource").textContent = data.recommended_capacity == null ? "No tested capacity qualifies" : data.resource;
  $("#result-utilization").textContent = `${(100 * display.summary.metrics.utilization.mean).toFixed(1)}%`;
  $("#result-p95").textContent = display.summary.metrics.p95_flow.mean.toFixed(1);
  $("#results-chart-title").textContent = `${data.objective.metric === "throughput" ? "Throughput" : "Mean flow time"} by capacity`;
  $("#results-chart-description").textContent = `${data.objective.metric === "throughput" ? "Higher" : "Lower"} is better. Gold is the smallest confidence-qualified capacity.`;
  const maximum = Math.max(data.objective.target, ...results.map(item => item.summary.metrics[key].mean + item.summary.metrics[key].ci95), .0001);
  $("#results-chart").innerHTML = results.map((item, index) => {
    const current = item.summary.metrics[key];
    const height = Math.max(8, current.mean / maximum * 220);
    const previous = index > 0 ? results[index - 1].summary.metrics[key].mean : null;
    const gain = previous == null ? null : (data.objective.metric === "throughput" ? current.mean - previous : previous - current.mean);
    return `<div class="bar-column ${item.meets_objective ? "qualifies" : ""}"><span class="bar-value">${current.mean.toFixed(2)} ± ${current.ci95.toFixed(2)}</span><div class="bar ${item.count === data.recommended_capacity ? "best" : ""}" style="height:${height}px"></div><span class="bar-label">${escapeHtml(data.resource)} ${item.count}</span><small>${(100 * item.summary.metrics.utilization.mean).toFixed(0)}% util${gain == null ? " · baseline" : ` · gain ${gain.toFixed(2)}`}</small></div>`;
  }).join("");
}

function loadReplay(data) {
  if (data?.replay_version !== 1 || !Array.isArray(data.transitions) || !Array.isArray(data.resources)) throw new Error("Unsupported replay");
  replayData = data;
  replayTime = 0;
  replaySelectedStage = data.stages[0]?.id || 0;
  replayTransitionsByTime = new Map();
  data.transitions.forEach(item => {
    if (!replayTransitionsByTime.has(item.time)) replayTransitionsByTime.set(item.time, []);
    replayTransitionsByTime.get(item.time).push(item);
  });
  replayEventTimes = [...replayTransitionsByTime.keys()].map(Number).sort((a, b) => a - b);
  replayEntityStates = new Map();
  replayAppliedTransition = 0;
  replayAppliedTime = -1;
  replayTimelineCache = new Map();
  replayAssignmentStages = new Map(data.transitions.filter(item => item.accepted && item.resource >= 0 && item.instance >= 0 && item.action === "acquire_and_process").map(item => [`${item.time}:${item.resource}:${item.instance}:${item.entity}`, item.stage]));
  $("#resource-timeline").innerHTML = "";
  delete $("#resource-timeline").dataset.filter;
  $("#replay-empty").hidden = true;
  $("#replay-workspace").hidden = false;
  $("#replay-scrubber").max = Math.max(1, Number(data.scenario.end_time || 0));
  $("#replay-scrubber").value = 0;
  $("#replay-resource-filter").innerHTML = `<option value="">All</option>${data.resources.map(resource => `<option value="${resource.id}">${escapeHtml(resource.name)}</option>`).join("")}`;
  renderReplay();
}

function replayTransitionsAtOrBefore(time) {
  if (time < replayAppliedTime) {
    replayEntityStates.clear();
    replayAppliedTransition = 0;
  }
  while (replayAppliedTransition < replayData.transitions.length && replayData.transitions[replayAppliedTransition].time <= time) {
    const transition = replayData.transitions[replayAppliedTransition++];
    replayEntityStates.set(transition.entity, transition);
  }
  replayAppliedTime = time;
  return replayEntityStates;
}

function latestReplayEventTime(time) {
  let low = 0;
  let high = replayEventTimes.length - 1;
  let found = null;
  while (low <= high) {
    const middle = (low + high) >> 1;
    if (replayEventTimes[middle] <= time) { found = replayEventTimes[middle]; low = middle + 1; }
    else high = middle - 1;
  }
  return found;
}

function renderReplayFlow(entityStates) {
  const filterText = $("#replay-entity-filter").value;
  const filter = filterText === "" ? null : Number(filterText);
  const counts = new Map();
  const dots = new Map();
  for (const entity of replayData.entities) {
    if (entity.entry_time > replayTime || (entity.completion_time >= 0 && entity.completion_time <= replayTime)) continue;
    if (filter != null && entity.id !== filter) continue;
    const transition = entityStates.get(entity.id);
    if (!transition) continue;
    const key = `${transition.stage}:${transition.to_state}`;
    counts.set(key, (counts.get(key) || 0) + 1);
    if (!dots.has(transition.stage)) dots.set(transition.stage, []);
    if (dots.get(transition.stage).length < 12) dots.get(transition.stage).push(entity.id);
  }
  const latestEventTime = latestReplayEventTime(replayTime);
  const pulseWindow = Math.max(1, replayData.scenario.end_time / 120);
  const activeTransitions = latestEventTime != null && replayTime - latestEventTime <= pulseWindow
    ? (replayTransitionsByTime.get(latestEventTime) || []).filter(item => item.accepted) : [];
  $("#replay-flow-map").innerHTML = `<div class="replay-stage-row">${replayData.stages.map(stage => {
    const stateCounts = stage.states.map((name, index) => ({ name, count: counts.get(`${stage.id}:${index}`) || 0 })).filter(item => item.count);
    const active = activeTransitions.some(item => item.stage === stage.id);
    return `<button class="replay-stage ${active ? "active" : ""} ${stage.id === replaySelectedStage ? "selected" : ""}" data-replay-stage="${stage.id}"><strong>${escapeHtml(stage.name)}</strong><span>${stateCounts.reduce((sum, item) => sum + item.count, 0)} active</span><small>${stateCounts.map(item => `${escapeHtml(item.name)} ${item.count}`).join(" · ") || "Empty"}</small><div class="entity-dots">${(dots.get(stage.id) || []).map(id => `<i title="Entity ${id}">${id}</i>`).join("")}</div></button>`;
  }).join("")}</div><div class="replay-routes">${replayData.stages.flatMap(stage => stage.outcomes.filter(outcome => outcome.next_stage >= 0).map(outcome => {
    const active = activeTransitions.some(item => item.stage === stage.id && item.outcome >= 0 && stage.outcomes[item.outcome]?.next_stage === outcome.next_stage);
    return `<span class="${active ? "active" : ""}">${escapeHtml(stage.name)} → ${escapeHtml(replayData.stages[outcome.next_stage]?.name || "Exit")}</span>`;
  })).join("")}</div>`;
  $$('[data-replay-stage]').forEach(button => button.addEventListener("click", event => { replaySelectedStage = Number(event.currentTarget.dataset.replayStage); renderReplay(); }));
  const activeCount = [...counts.values()].reduce((sum, value) => sum + value, 0);
  $("#replay-summary").textContent = `${activeCount} active work item${activeCount === 1 ? "" : "s"}; ${replayData.scenario.completed} completed by the end.`;
}

function renderReplayFsm() {
  const stage = replayData.stages.find(item => item.id === replaySelectedStage) || replayData.stages[0];
  if (!stage) { $("#replay-fsm").innerHTML = `<p class="empty-state">No stages.</p>`; return; }
  let recent = null;
  for (let index = replayAppliedTransition - 1; index >= 0; index--) {
    if (replayData.transitions[index].stage === stage.id) { recent = replayData.transitions[index]; break; }
  }
  $("#replay-fsm").innerHTML = `<h3>${escapeHtml(stage.name)}</h3><div class="fsm-state-row">${stage.states.map((state, index) => `<span class="fsm-state ${index === stage.initial_state ? "initial" : ""} ${recent?.to_state === index ? "active" : ""}">${escapeHtml(state)}</span>`).join("")}</div><div class="fsm-arrow-list">${(stage.fsm || []).map(transition => `<span class="${recent && recent.from_state === transition.state && recent.event === transition.event ? "active" : ""}"><strong>${escapeHtml(stage.states[transition.state])}</strong> —${escapeHtml(stage.events[transition.event])}→ <strong>${escapeHtml(stage.states[transition.next_state])}</strong><small>${escapeHtml(transition.action)}</small></span>`).join("")}</div>`;
}

function timelineSegments(resource, instance) {
  const cacheKey = `${resource.id}:${instance}`;
  if (replayTimelineCache.has(cacheKey)) return replayTimelineCache.get(cacheKey);
  const end = Math.max(1, replayData.scenario.end_time);
  const changes = replayData.resource_timeline.filter(item => item.resource === resource.id && item.instance === instance).sort((a, b) => a.time - b.time);
  const segments = [];
  let cursor = 0;
  if (resource.available_at > 0) {
    segments.push({ start: 0, end: Math.min(end, resource.available_at), kind: "unavailable", entity: -1 });
    cursor = Math.min(end, resource.available_at);
  }
  let kind = "idle";
  let entity = -1;
  let stage = -1;
  for (const change of changes) {
    if (change.time > cursor) segments.push({ start: cursor, end: change.time, kind, entity, stage });
    cursor = change.time; kind = change.busy ? "busy" : "idle"; entity = change.entity;
    stage = change.busy ? (replayAssignmentStages.get(`${change.time}:${resource.id}:${instance}:${change.entity}`) ?? -1) : -1;
  }
  if (cursor < end) segments.push({ start: cursor, end, kind, entity, stage });
  replayTimelineCache.set(cacheKey, segments);
  return segments;
}

function renderResourceTimeline() {
  if (!replayData) return;
  const filter = $("#replay-resource-filter").value;
  const resources = replayData.resources.filter(resource => filter === "" || resource.id === Number(filter));
  const end = Math.max(1, replayData.scenario.end_time);
  const timeline = $("#resource-timeline");
  if (timeline.dataset.filter !== filter || !timeline.children.length) {
    timeline.dataset.filter = filter;
    timeline.innerHTML = resources.flatMap(resource => Array.from({ length: resource.instances }, (_, instance) => {
    const segments = timelineSegments(resource, instance);
      return `<div class="timeline-row"><span class="timeline-label">${escapeHtml(resource.name)} #${instance + 1}</span><div class="timeline-track">${segments.map(segment => `<span class="timeline-segment ${segment.kind}" style="left:${segment.start / end * 100}%;width:${Math.max(.12, (segment.end - segment.start) / end * 100)}%" title="${segment.kind}${segment.entity >= 0 ? ` · entity ${segment.entity}` : ""}${segment.stage >= 0 ? ` · ${escapeHtml(replayData.stages[segment.stage]?.name)}` : ""} · ${segment.start}–${segment.end}"></span>`).join("")}<i class="timeline-cursor"></i></div></div>`;
    })).join("") || `<p class="empty-state">No resource lanes match this filter.</p>`;
  }
  timeline.querySelectorAll(".timeline-cursor").forEach(cursor => { cursor.style.left = `${replayTime / end * 100}%`; });
}

function renderReplayEventLog() {
  const eventTime = latestReplayEventTime(replayTime);
  const events = eventTime == null ? [] : (replayTransitionsByTime.get(eventTime) || []);
  $("#replay-event-log").innerHTML = events.slice(0, 100).map(event => {
    const stage = replayData.stages[event.stage];
    const eventName = stage?.events[event.event] || `event ${event.event}`;
    return `<span><strong>Entity ${event.entity}</strong> · ${escapeHtml(stage?.name || "Unknown")} · ${escapeHtml(eventName)} · ${escapeHtml(event.action)} · ${event.accepted ? `${escapeHtml(stage?.states[event.from_state])} → ${escapeHtml(stage?.states[event.to_state])}` : "waiting/retry"}</span>`;
  }).join("") || `<span>No transition has occurred yet.</span>`;
}

function renderReplay() {
  if (!replayData) return;
  $("#replay-time-label").textContent = `t = ${replayTime.toFixed(1)} / ${replayData.scenario.end_time}`;
  $("#replay-scrubber").value = replayTime;
  const states = replayTransitionsAtOrBefore(replayTime);
  renderReplayFlow(states);
  renderReplayFsm();
  renderResourceTimeline();
  renderReplayEventLog();
}

function stopReplay() {
  replayPlaying = false;
  $("#replay-play").textContent = "Play";
  if (replayFrame) cancelAnimationFrame(replayFrame);
  replayFrame = null;
}

function animateReplay(timestamp) {
  if (!replayPlaying || !replayData) return;
  if (!replayLastFrame) replayLastFrame = timestamp;
  const delta = timestamp - replayLastFrame;
  replayLastFrame = timestamp;
  const speed = Number($("#replay-speed").value || 1);
  replayTime = Math.min(replayData.scenario.end_time, replayTime + delta * replayData.scenario.end_time / 20000 * speed);
  renderReplay();
  if (replayTime >= replayData.scenario.end_time) stopReplay();
  else replayFrame = requestAnimationFrame(animateReplay);
}

function stepReplay(direction) {
  if (!replayData) return;
  stopReplay();
  if (direction > 0) replayTime = replayEventTimes.find(time => time > replayTime + 1e-9) ?? replayData.scenario.end_time;
  else replayTime = [...replayEventTimes].reverse().find(time => time < replayTime - 1e-9) ?? 0;
  renderReplay();
}

function renderAll() {
  $("#scenario-name").value = scenario.name;
  $("#model-heading").textContent = scenario.name;
  renderResources(); renderCanvas(); renderInspector(); updateCommand(); validate();
}

$$('.tab').forEach(tab => tab.addEventListener("click", () => {
  $$('.tab').forEach(item => item.classList.toggle("active", item === tab));
  $$('.view').forEach(view => view.classList.remove("active"));
  $(`#${tab.dataset.view}-view`).classList.add("active");
  if (tab.dataset.view === "model") requestAnimationFrame(drawCanvasLinks);
}));
$("#scenario-name").addEventListener("change", event => { scenario.name = event.target.value.trim() || "Untitled scenario"; renderAll(); });
$("#add-resource").addEventListener("click", () => { let number = scenario.resources.length + 1; let name = `Resource_${number}`; while (scenario.resources.some(item => item.name === name)) name = `Resource_${++number}`; scenario.resources.push({ name, count: 1, available_at: 0 }); renderAll(); });
$("#add-stage").addEventListener("click", () => { let number = scenario.stages.length + 1; let name = `Stage_${number}`; while (scenario.stages.some(item => item.name === name)) name = `Stage_${++number}`; scenario.stages.push({ name, mode: "resource", resource: scenario.resources[0]?.name, processing_time: { distribution: "fixed", param1: 10, param2: 0 }, outcomes: [{ name: "DONE", probability: 1, next_stage: null }] }); selectedStage = scenario.stages.length - 1; renderAll(); });
$("#fit-flow").addEventListener("click", () => $("#process-canvas").scrollTo({ left: 0, behavior: "smooth" }));
$("#export-button").addEventListener("click", () => {
  if (validate().length) return toast("Resolve validation issues before exporting");
  const blob = new Blob([JSON.stringify(scenario, null, 2)], { type: "application/json" });
  const link = document.createElement("a"); link.href = URL.createObjectURL(blob); link.download = "scenario.json"; link.click(); URL.revokeObjectURL(link.href);
});
$("#import-file").addEventListener("change", async event => { try { scenario = normalizeScenario(JSON.parse(await event.target.files[0].text())); selectedStage = 0; renderAll(); toast("Scenario imported"); } catch { toast("That file is not a valid scenario"); } });
$("#results-file").addEventListener("change", async event => { try { renderResults(JSON.parse(await event.target.files[0].text())); toast("Results imported"); } catch { toast("That file does not contain version-2 sweep results"); } });
$("#replay-file").addEventListener("change", async event => { try { loadReplay(JSON.parse(await event.target.files[0].text())); toast("Replay imported"); } catch { toast("That file does not contain a version-1 replay"); } });
['runs','seed','objective-metric','objective-target','sweep-resource','sweep-min','sweep-max'].forEach(id => $(`#${id}`).addEventListener("input", updateCommand));
$("#copy-command").addEventListener("click", () => copyText($("#command-preview").textContent, "Command copied"));
$("#copy-replay-command").addEventListener("click", () => copyText($("#replay-command").textContent, "Replay command copied"));
$("#replay-play").addEventListener("click", () => { if (!replayData) return; if (replayPlaying) return stopReplay(); if (replayTime >= replayData.scenario.end_time) replayTime = 0; replayPlaying = true; replayLastFrame = 0; $("#replay-play").textContent = "Pause"; replayFrame = requestAnimationFrame(animateReplay); });
$("#replay-prev").addEventListener("click", () => stepReplay(-1));
$("#replay-next").addEventListener("click", () => stepReplay(1));
$("#replay-scrubber").addEventListener("input", event => { stopReplay(); replayTime = Number(event.target.value); renderReplay(); });
$("#replay-resource-filter").addEventListener("change", renderResourceTimeline);
$("#replay-entity-filter").addEventListener("input", renderReplay);

async function copyText(value, message) {
  try { await navigator.clipboard.writeText(value); }
  catch { const area = document.createElement("textarea"); area.value = value; document.body.append(area); area.select(); document.execCommand("copy"); area.remove(); }
  toast(message);
}

window.addEventListener("resize", () => requestAnimationFrame(drawCanvasLinks));
renderAll();
