# Work to Be Done

This is the ordered implementation backlog for the native C++ reimplementation. Work should proceed as complete,
testable vertical slices rather than as a bank-by-bank translation of the 6502 program.

Related design documents:

- [Native architecture](../port/ARCHITECTURE.md)
- [Behavior reconstruction rules](../port/BEHAVIOR_NOTES.md)
- [Current port status](../port/PORTING_STATUS.md)
- [Routine-to-native traceability](../port/routine-map.tsv)

## Non-Negotiable Rules

- [ ] Never call, link, interpret, or otherwise execute the original assembly from the native program.
- [ ] Keep `DW4.Game` deterministic and independent of SDL, filesystems, rendering, audio, and reference tools.
- [ ] Use the disassembly, routine contracts, traces, and extracted data as evidence, not as the native architecture.
- [ ] Preserve player-observable behavior and data relationships; binary-identical 6502 output is not a goal.
- [ ] Do not commit ROMs, extracted assets, save files, emulator captures, or packaged copies of those files.
- [ ] Do not weaken or bypass the native-boundary or copyright-boundary checks to make a build pass.
- [ ] Do not add placeholder gameplay, inert controls, temporary art, or fabricated data to claim progress.
- [ ] Finish and validate one vertical slice before opening the next one.

## Definition of Done for Every Gameplay Slice

- [ ] Identify the controlling reference routines, data ranges, contracts, and runtime observations.
- [ ] Write the player-observable behavior contract before implementing it.
- [ ] Add the native mapping and evidence citation to `docs/port/routine-map.tsv`.
- [ ] Load required game data through typed `DW4.Content` records; do not parse JSON in `DW4.Game`.
- [ ] Implement deterministic game rules in `DW4.Game` and platform presentation in `DW4.Desktop`.
- [ ] Add unit tests for rules and an integration or replay test for the complete interaction.
- [ ] Compare native results against reference traces or captures at explicit checkpoints.
- [ ] Exercise keyboard and controller paths, save/load where applicable, and error handling.
- [ ] Update `docs/port/PORTING_STATUS.md` without narrowing the remaining scope.
- [ ] Pass `test-native.cmd`, the native-boundary check, and the copyright-boundary check.

## Phase 0 - Repository and Toolchain Foundation

- [x] Move the existing disassembly, analysis, tools, and documentation intact under `reference/`.
- [x] Build the moved reference solution and reproduce the exact expected ROM SHA-256.
- [x] Create `DragonWarrior4.Native.sln` with `DW4.Game`, `DW4.Content`, `DW4.Desktop`, and `DW4.Tests`.
- [x] Configure C++23, x64 Debug/Release, strict warnings, SDL3, SDL3_image, nlohmann-json, and Catch2.
- [x] Add deterministic frame/input primitives at the NES NTSC cadence.
- [x] Add native project checks that reject assembly inputs and dependencies on the reference implementation.
- [x] Add copyright checks for ROMs, extracted assets, captures, archives, and packaged copies.
- [x] Add `build-native.cmd`, `test-native.cmd`, and `extract-native-assets.cmd`.
- [x] Verify the asset extractor can consume the exact rebuilt ROM and write to an ignored directory.
- [x] Add an initial game unit test and content integration tests.

## Phase 1 - Evidence, Content, and Parity Baseline

Complete this before implementing gameplay so later behavior can be measured rather than judged by appearance alone.

### Asset Management

- [ ] Run every extraction stage and inventory the resulting text, maps, sprites, monsters, graphics, screens, and audio.
- [ ] Verify every generated file remains ignored, including runtime/package copies created during development.
- [ ] Document which outputs are runtime inputs and which are reference-only evidence.
- [ ] Treat captured screens as comparison evidence, not as background images for the native game.
- [ ] Define versioned, authored schemas for each JSON document consumed by the native runtime.
- [ ] Validate required files, schema versions, identifiers, dimensions, palette indexes, and cross-references at startup.
- [ ] Produce clear diagnostics for missing, stale, malformed, or partially extracted asset sets.
- [ ] Convert JSON documents into immutable typed records in `DW4.Content`.
- [ ] Keep all ROM addresses and extraction-specific details inside the content/evidence boundary.
- [ ] Add integration tests using small authored fixtures rather than committed extracted payloads.
- [ ] Decide how installed builds locate user-extracted assets without copying them into Git or release archives.

### Reference Capture and Replay

- [ ] Define a versioned replay format containing initial state, RNG state, and one controller sample per game frame.
- [ ] Define stable native snapshots for flags, party, inventory, location, field position, battle state, and RNG.
- [ ] Add a reference capture workflow for the same checkpoints using existing FCEUX traces and save evidence.
- [ ] Add comparison tooling that reports the first divergent frame and field instead of only pass/fail.
- [ ] Capture the title/new-game/opening sequence needed by the first vertical slice.
- [ ] Keep captures containing copyrighted screen/audio/save payloads ignored; track only authored expectations and tooling.

### Deterministic Core

- [ ] Identify and implement the game's random-number behavior with explicit state ownership.
- [ ] Separate simulation ticks from rendering cadence and wall-clock time.
- [ ] Define scene/state transitions without recreating the NES RAM map.
- [ ] Add serialization-independent snapshots for tests and replay comparison.
- [ ] Verify repeated runs with the same initial state and input replay produce identical snapshots.

## Phase 2 - Platform Services Required by the First Slice

Implement these capabilities through the first gameplay slice. Do not build unused general-purpose engine features.

### Graphics Rendering

- [ ] Create a 256 by 240 logical framebuffer with integer scaling, pillar/letterboxing, and resize handling.
- [ ] Establish explicit draw ordering for backgrounds, windows, text, sprites, fades, and overlays.
- [ ] Load indexed PNGs without losing palette or transparency semantics.
- [ ] Implement NES palette conversion and runtime palette selection from extracted metadata.
- [ ] Implement 8 by 8 tiles, 16 by 16 metatiles, sprite frames, horizontal/vertical flips, and clipping.
- [ ] Implement the main and alternate fonts with the original text-code mapping.
- [ ] Implement reusable menu/window borders, cursors, numeric fields, and text layout.
- [ ] Implement fade-in, fade-out, blanking, and scene transitions required by the title/opening flow.
- [ ] Add render-command tests that do not require a GPU.
- [ ] Add screenshot comparison with documented tolerances for the first slice.

### Input Handling

- [ ] Represent current, pressed, released, and repeated button states per simulation frame.
- [ ] Map keyboard and SDL gamepads to A, B, Start, Select, and the directional pad.
- [ ] Implement the reference menu-repeat timing and opposing-direction behavior.
- [ ] Handle controller connection/disconnection without changing deterministic game state.
- [ ] Add configurable bindings only after the original control path is complete.
- [ ] Test keyboard, controller, focus loss, and held-button transitions.

### Audio Playback

- [ ] Load extracted WAV data and loop metadata through `DW4.Content`.
- [ ] Implement SDL3 audio-device ownership and format conversion.
- [ ] Support music intro/loop boundaries without audible gaps.
- [ ] Support concurrent sound effects, interruption, priority, and channel behavior required by the first slice.
- [ ] Map game music/SFX identifiers to typed content records instead of filenames in gameplay code.
- [ ] Use VGM captures as fidelity evidence; do not execute the original sound engine at runtime.
- [ ] Add headless tests for event scheduling, loop points, and music/SFX state transitions.

### Save System Foundation

- [ ] Derive adventure-log semantics, defaults, validation, and copy/delete behavior from save-RAM evidence.
- [ ] Define a versioned native save container separate from in-memory game structures.
- [ ] Store saves in the operating system's per-user data directory, never in tracked source/assets paths.
- [ ] Use atomic write/replace behavior and preserve the previous valid save on failure.
- [ ] Detect corruption and unsupported versions with a recoverable user-facing error.
- [ ] Add deterministic new-game initialization and round-trip tests for every field introduced by the first slice.
- [ ] Defer NES save import/export until native save semantics are complete and tested.

### Desktop Runtime

- [ ] Replace the blank host with explicit scene presentation driven by `DW4.Game` frame output.
- [ ] Define renderer, audio, input, save, and asset service interfaces at the platform boundary.
- [ ] Add clean startup/shutdown and actionable errors for assets, video, audio, and save storage.
- [ ] Pause or constrain wall-clock accumulation when the process loses focus or resumes after a stall.
- [ ] Add basic runtime options for integer scale, fullscreen/windowed mode, volume, and input bindings when functional.

## Phase 3 - First Complete Vertical Slice

Target: launch the game, reproduce the title/adventure-log flow, create a new game, and enter the first controllable
opening scene with correct rendering, input, audio, dialogue, event flags, and persistence.

### Title and Adventure Log

- [ ] Reproduce title-screen composition, animation, timing, music, and accepted inputs.
- [ ] Implement every command exposed by the title/adventure-log UI; do not ship inert menu entries.
- [ ] Implement empty/occupied slot presentation from actual native save state.
- [ ] Implement cursor movement, cancellation, confirmation, transitions, and sound effects.
- [ ] Implement save creation, selection, copy/delete, and validation behaviors evidenced by the reference game.

### Name Entry and New Game

- [ ] Reproduce the name-entry grid, character rules, cursor behavior, editing, confirmation, and cancellation.
- [ ] Validate name length and text encoding through typed game rules.
- [ ] Initialize all chapter, party, inventory, flag, RNG, location, and save fields required for a new game.
- [ ] Persist the new adventure log and verify it can be loaded after process restart.

### Opening Playable Scene

- [ ] Load and render the correct opening map/submap, palette, entities, and initial camera position.
- [ ] Implement player movement, facing, animation, collision, and map boundaries needed in the opening area.
- [ ] Implement the opening event sequence and all state changes reachable before leaving the slice boundary.
- [ ] Implement dialogue boxes, control codes, paging, choice prompts, and dynamic values used by the opening scene.
- [ ] Implement the command/menu interactions that are reachable in the opening scene.
- [ ] Save, quit, reload, and resume at the correct state and position.
- [ ] Add a deterministic replay from launch to first player control and compare all defined checkpoints.

### First-Slice Acceptance

- [ ] Complete the full flow without developer commands, hardcoded skips, placeholder data, or inert controls.
- [ ] Match reference checkpoints for frames where timing matters, game state, selected audio, and presentation.
- [ ] Pass a clean-checkout workflow: extract assets locally, build, test, run, create a save, and reload it.
- [ ] Update the status and traceability documents before beginning general field exploration.

## Phase 4 - Field Exploration Slice

Target: travel through connected field maps with correct menus, dialogue, events, encounters, and persistence.

### Maps and Movement

- [ ] Decode typed location, world-map, tileset, palette, roof, and entity records from extracted metadata.
- [ ] Render maps from cells/metatiles rather than using full-map PNG previews as gameplay backgrounds.
- [ ] Implement camera scrolling, submap transitions, doors, stairs, warps, and world/location transitions.
- [ ] Implement terrain collision, directional restrictions, counters, damage tiles, and scripted blockers.
- [ ] Implement roofs/interiors, day/night palette behavior, and animated map tiles.
- [ ] Implement field sprite selection, direction, step animation, palette changes, rotation, and visibility rules.

### Dialogue and Field Interaction

- [ ] Implement all dialogue symbols and control codes with reference line wrapping and pagination.
- [ ] Implement speaker names, choices, numeric values, item/spell/place names, and context-dependent handlers.
- [ ] Implement talk, search, door, stair, inspect, and event-trigger interactions.
- [ ] Implement NPC movement and interaction timing required by reachable maps.
- [ ] Build a native event/state model for flags and scripted sequences without interpreting 6502 code.

### Field Menus and Progression

- [ ] Implement party status, items, equipment, spells, tactics, and formation menus.
- [ ] Implement item use, transfer, equip/unequip, discard, and inventory-capacity rules.
- [ ] Implement spell eligibility, costs, targeting, effects, and field restrictions.
- [ ] Implement shops, inns, churches, banks/vaults, and other standard location services.
- [ ] Implement gold, experience, levels, derived statistics, and learned-spell progression.
- [ ] Add deterministic tests for each menu action and persistence round trip.

### Encounters and World Travel

- [ ] Implement encounter zones, terrain modifiers, step accounting, RNG consumption, and encounter selection.
- [ ] Implement world-map traversal and every vehicle/transport mode when first reached by a complete slice.
- [ ] Implement day/night or progression-dependent world changes when first encountered.
- [ ] End this phase with a replay that travels between multiple locations and reaches a battle transition.

## Phase 5 - Battle Slice

Target: enter, resolve, and exit representative normal and boss battles with exact game-state outcomes.

### Battle Rules

- [ ] Define typed combatants, parties, formations, commands, actions, effects, resistances, and status conditions.
- [ ] Implement encounter formation and battle initialization.
- [ ] Implement command selection, targeting, tactics, turn ordering, agility, and RNG consumption.
- [ ] Implement physical attacks, criticals, defense, damage formulas, misses, and special attack properties.
- [ ] Implement spells, breath/actions, items, status changes, resistances, immunities, reflection, and absorption.
- [ ] Implement monster AI patterns, weighted actions, summons, transformations, and phase changes.
- [ ] Implement victory, defeat, escape, experience, gold, drops, level-up, and post-battle state restoration.
- [ ] Cover chapter-specific rules and special-party behavior only when supported by cited evidence.

### Battle Presentation

- [ ] Render battle backgrounds, monster compositions, palettes, presentation patches, and animations.
- [ ] Render party/command/status windows, messages, damage values, targeting, and state changes.
- [ ] Implement battle music transitions and action/victory/defeat sound effects.
- [ ] Preserve presentation timing where it controls input windows, action order, or visible outcomes.

### Battle Verification

- [ ] Create authored fixtures for representative attacks, spells, statuses, AI patterns, drops, and escape attempts.
- [ ] Compare native turn snapshots and RNG state against reference traces.
- [ ] Add complete replays for one normal encounter, one multi-monster encounter, and one scripted boss.
- [ ] Require zero unexplained state divergence before expanding battle content.

## Phase 6 - Chapter and World Completion

Implement complete playable progression in story order so every newly exposed system is finished when introduced.

- [ ] Complete Chapter 1 progression, events, required battles, services, and chapter transition.
- [ ] Complete Chapter 2 progression and newly introduced party/arena systems.
- [ ] Complete Chapter 3 progression and chapter-specific economy/drop behavior.
- [ ] Complete Chapter 4 progression and newly introduced travel/event systems.
- [ ] Complete Chapter 5 assembly of the final party, wagon behavior, tactics, vehicles, and world progression.
- [ ] Implement all towns, castles, shrines, caves, towers, dungeons, world variants, and transition rules.
- [ ] Implement all recruitments, party changes, temporary members, deaths, transformations, and event overrides.
- [ ] Implement all bosses, scripted battles, special victory/defeat handling, and final battle phases.
- [ ] Implement Zenithia/endgame progression, ending sequence, staff credits, and return/termination behavior.
- [ ] Add a versioned save migration whenever completed progression changes persisted data.
- [ ] Maintain at least one deterministic replay/checkpoint chain per chapter.

## Phase 7 - Optional and Specialized Systems

These are required for completion even when they are not on the shortest story path.

- [ ] Implement casino slot machines, poker, coins, rewards, and persistence.
- [ ] Implement tournament/arena flows and other chapter-specific battle presentations.
- [ ] Implement Small Medal collection and reward progression.
- [ ] Implement shops or services with specialized inventory, pricing, negotiation, or chapter rules.
- [ ] Implement hidden items, optional maps, side events, optional recruits, and alternate dialogue states.
- [ ] Implement all tactics modes and AI-controlled party behavior.
- [ ] Audit every extracted item, spell, monster, map, sprite set, music track, and sound effect for a runtime use or a
  documented reason it is not used.

## Phase 8 - Save Compatibility and Resilience

- [ ] Finalize the native save schema after all persistent gameplay systems exist.
- [ ] Test all save slots, chapter boundaries, event flags, party variants, inventories, vehicles, casino state, and ending state.
- [ ] Add migration tests from every previously released native save version.
- [ ] Add backup recovery and interrupted-write tests.
- [ ] Decide whether NES `.sav` import/export is a supported feature.
- [ ] If supported, implement it as an offline conversion tool with exact field validation and no runtime ROM dependency.
- [ ] Keep all real and generated save files ignored and out of test fixtures.

## Phase 9 - Full-Game Parity and Completion Audit

- [ ] Build an inventory of every meaningfully named reference routine and map it to native behavior or a documented
  non-runtime concern; do not require one C++ function per routine.
- [ ] Audit all routine contracts used by gameplay and cite them from native tests or traceability entries.
- [ ] Run full chapter replays and compare checkpoints for progression, party, inventory, flags, RNG, and save state.
- [ ] Run focused parity suites for field movement, menus, economy, encounters, combat, AI, events, and ending behavior.
- [ ] Compare representative screens, animation timings, music loops, and sound-effect sequences.
- [ ] Resolve every unexplained divergence or document an intentional native-platform deviation in `BEHAVIOR_NOTES.md`.
- [ ] Test clean startup and actionable failure modes with missing, partial, stale, and corrupt extracted assets.
- [ ] Test extended play, repeated save/load, device reconnects, focus changes, and scene transitions for leaks or drift.
- [ ] Confirm no native binary imports or dynamically loads a reference tool, ROM, emulator, or assembly payload.
- [ ] Confirm Git history and release staging contain no ROM, extracted asset, capture, or save payload.

## Phase 10 - Packaging and Release

- [ ] Define the supported Windows versions, architectures, Visual C++ runtime strategy, and GPU/audio requirements.
- [ ] Produce a release build containing only native binaries, required third-party runtime libraries, licenses, and docs.
- [ ] Do not include a ROM, extracted assets, saves, traces, screenshots, audio, or reference build products.
- [ ] Provide a first-run asset setup flow that asks the user to build/extract from their own lawful ROM source.
- [ ] Validate asset-set compatibility without retaining or uploading user ROM data.
- [ ] Add version information, crash/error logging, and a diagnostics report that excludes copyrighted payloads.
- [ ] Run the full build, test, copyright, replay, save-migration, and packaging gates from a clean checkout.
- [ ] Document installation, asset preparation, controls, save location, troubleshooting, and uninstall behavior.
- [ ] Tag a release only when the complete game is playable from title screen through ending with no placeholder path.

This roadmap is complete only when every required gameplay path and release gate above is finished and validated.
