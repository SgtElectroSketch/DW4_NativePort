# Behavior Reconstruction Notes

## Goal

Preserve player-observable rules, data relationships, timing where it affects play, presentation, and save semantics.
Matching 6502 instruction order, CPU addresses, mapper operations, or binary output is not a native-port requirement.

## Evidence Order

When sources appear to disagree, use this order:

1. Repeatable runtime traces and captured state transitions.
2. Reviewed routine contracts and call/interface evidence.
3. Named disassembly routines and their callers.
4. Extracted data records and presentation metadata.
5. Human-authored notes, used only when consistent with stronger evidence.

## Porting Rule

For each behavior:

1. Identify the controlling reference routines and data.
2. Write the observable contract without encoding incidental RAM addresses.
3. Capture a minimal input/state example that distinguishes the behavior.
4. Implement the contract in the owning C++ domain.
5. Add a deterministic test or replay comparison.
6. Record the mapping in `routine-map.tsv`.

Do not translate banks one at a time and do not create a C++ version of the NES memory map. A native function may
replace several cooperating 6502 routines, while one large routine may become several domain operations.

## Exactness Categories

- **Rule exactness:** damage, movement, inventory, flags, encounter selection, AI, and progression outcomes.
- **Data exactness:** IDs, names, statistics, map cells, palettes, frame ordering, and lookup relationships.
- **Presentation fidelity:** 256 by 240 composition, sprite/tile placement, text flow, animation cadence, and audio loops.
- **Platform adaptation:** windowing, controller APIs, filesystem paths, save containers, and display scaling are native.

Any deliberate deviation belongs in this document and must not be hidden by changing the reference evidence.
