# Native Port Status

## Implemented

- Existing disassembly, analysis, scripts, and tools moved intact under `reference/`.
- Reference solution builds from its new location and reproduces the exact expected SHA-256.
- Visual Studio native solution with `DW4.Game`, `DW4.Content`, `DW4.Desktop`, and `DW4.Tests`.
- C++23, x64 Debug/Release, strict warnings, pinned vcpkg dependencies, and SDL3 deployment.
- Build-time rejection of assembly inputs and references outside native/test ownership.
- Deterministic frame/input foundation at the NES NTSC cadence.
- Deterministic held, pressed, and released button masks with value-based frame snapshots and repeated-input tests.
- Approved modern RNG using `std::mt19937` and standard uniform distributions, with explicit snapshot-owned engine state.
- Bounded foundation replay execution, sparse checkpoints, snapshot restoration, and first-frame/field divergence reporting.
- Versioned replay JSON persistence using nlohmann-json and standard engine serialization with toolchain compatibility checks.
- Headless replay execution through `DW4.Desktop.exe --replay <file>` and authored matching/divergent command smoke tests.
- Startup validation of schema-version-1 object roots and all nine required record collections across seven catalog families.
- Immutable typed raw-dialogue records, stable ID lookup, duplicate-ID checks, group/index consistency, and byte-range validation.
- Missing/unsupported catalog-version diagnostics and malformed-JSON errors that do not expose payload text.
- Headless current-contract validation through `DW4.Desktop.exe --validate-assets`.
- Local asset pipeline from exact ROM rebuild to ignored PNG/WAV/VGM/JSON/TSV files.
- Developer-only, atomic catalog preparation integrated into complete and staged asset extraction.
- Authored version-1 schemas for currently consumed fields; complete non-dialogue domain schemas remain unfinished.
- Native input/content regressions and authored preparation tests integrated into the supported test command.
- Configuration-specific Catch2 linkage for Debug and Release tests.
- Typed tile/font catalogs with validated image paths, formats, tile counts, unique glyph IDs, and immutable records.
- SDL3_image PNG feature, decoding of all fourteen real tile sheets and both fonts, exact indexed palette recovery,
    canonical image containment, dimension checks, and glyph-capacity checks.
- SDL3 logical-framebuffer renderer using palette textures, nearest scaling, bounded texture caching, transparency,
    horizontal/vertical flips, clipping, and explicit draw order.
- Deterministic metatile, window-border, and explicit glyph-grid command layout with coordinate validation.
- Native `--render-check <png>` inspection output; actual fonts/tile data are drawn rather than captured screenshots.
- Authored software pixel tests and repeated hidden-window render smoke checks, including seven integer-scaled
    desktop/ultrawide output dimensions.
- Native opening/title presentation from typed pattern, palette, draw and clip records, with animated idle scrolling.
- Developer-only title extraction with deduplicated patterns/draws, verified boundaries and observed music cues.
- Native title WAV playback through SDL3 audio streams; track 2 starts at observed simulation frame 349.
- Bounded native `--title-check`, `--title-frame`, and `--run-frames` verification commands.
- Pixel-exact title comparisons at frames 31, 183, 384, 673, and 1556, plus the verified 1,024-frame idle-tail period.
- SDL keyboard/gamepad polling with DualSense-compatible buttons, D-pad/left-stick normalization, hotplug handling,
  neutral-input focus resume, and actual SDL virtual-device integration tests.
- Deterministic menu press edges, configurable native directional repeat, and opposing-direction cancellation.
- Live title dismissal into native adventure-log command/slot/name/gender/speed/confirmation views built from extracted fonts.
- Conditional create/copy/erase/message-speed operations, with eight-glyph name editing and explicit storage completion/errors.
- Versioned native adventure-profile codec with zlib integrity, complete RNG continuation, atomic flushed three-slot storage,
  previous-save backup, corruption diagnostics and isolated create/copy/reopen/erase tests.
- Authored live-input script drove the normal loop through title dismissal, name entry and actual profile persistence.

## Not Yet Implemented

- Complete typed map, field-sprite, monster, palette/audio records and remaining payload/cross-reference checks.
- Scene-specific graphics/animation records, semantic dialogue layout, presentation timing, and GPU stress coverage.
- Gameplay RNG seed/consumption policy, complete domain snapshots/replays, and reference capture/parity comparisons.
- Opening gameplay/chapter startup and Continue integration; complete new-game party, inventory, flags and location initialization.
- Map rendering, collision, field entities, dialogue, events, and random encounters.
- Battle simulation and presentation.
- Music/SFX playback, save persistence, controller mapping, or configuration UI.
- Reference replay capture and behavior-parity gates.

The desktop executable is an infrastructure host at this stage, not a playable build.

It now renders the title and working adventure-log/profile menus. Continue remains hidden until its gameplay destination
exists. Keyboard defaults are preserved; gamepads/DualSense handling is now authorized and implemented at the SDL boundary.

## Verified Foundation Checkpoint

- Rebuilt the reference in this copy and verified the exact expected 524,304-byte ROM and SHA-256.
- Exercised the supported staged text-extraction command and prepared all seven local catalogs without changing reference sources.
- Release native tests: 889 assertions across 15 test cases; preparation tests: 37 assertions.
- Debug native tests: the same 889 assertions across 15 test cases, after correcting the pre-existing Catch2 library-name mismatch.
- Both configurations passed headless loading of the real schema-version-1 asset set.

## Modern RNG And Replay Increment

- The owner approved standard-library RNG instead of ROM-stream reproduction; the exact adaptation is recorded in `BEHAVIOR_NOTES.md`.
- The latest Release native suite passed 7,079 assertions across 28 cases, including RNG, snapshot continuation, replay codecs,
  first-divergence behavior, numeric/compatibility limits, malformed input, and authored field preservation.
- The real Release executable accepted a matching foundation replay and rejected the altered checkpoint at frame 73 with exit 2.
- The test entrypoint now rejects every nonzero native process exit, including negative Windows exception codes.
- A fresh Debug build passed the same 7,079 assertions across 28 cases and the matching/divergent executable replay checks.
- A copied/incremental Release link state produced test exception-path crashes; a fresh owned-project rebuild resolved them
    without changing source behavior or optimization. Verification uses the rebuilt binaries, not the stale outputs.

This checkpoint does not complete Phase 1 or claim gameplay parity. The original implementation backlog remains intact.

## Native Graphics Checkpoint

- Release and Debug passed 7,580 assertions across 35 native test cases.
- The supported Release workflow also passed catalog preparation, matching/divergent replay commands, and repeated
    render command smoke checks. Debug passed repeated render checks and loading of all fourteen real tile sheets plus both fonts.
- Software pixel tests cover exact palette recovery, alpha transparency, draw order, flips, signed clipping,
    layout bounds, and integer presentation at 256x240, 512x480, 768x720, 800x600, 1280x720, 1440x900, and 3200x1080.
- The real extracted fonts and recolored/flipped tile data were rendered natively into ignored 256x240 inspection output.
- This is a verified graphics subsystem, not title/new-game/opening gameplay or reference-scene parity.

## Native Title Presentation Checkpoint

- Debug and fresh Release builds passed 7,777 assertions across 40 native cases.
- Both configurations matched all 61,440 pixels at reference frames 31, 183, 384, 673, and 1556; late-frame playback
    retained the verified 1,024-frame scrolling period.
- Bounded live runs completed 600 simulation frames and started native WAV title music at frame 349.
- Repeated title draws are shared in the generated document; this removed the Debug startup timeout caused by repeated
    full tile records without extending the per-render process bound.
- Normal launch now displays opening/title animation. Existing keyboard/gamepad handling is unchanged; live title
    dismissal remains deferred until the complete adventure-log destination is implemented.
- This checkpoint does not complete the first playable title/new-game/opening vertical slice.

## Adventure Log And Input Checkpoint

- Debug and clean Release builds passed 7,869 assertions across 49 native cases.
- SDL virtual-gamepad tests exercised actual polling, buttons/axes, focus loss/regain, fresh re-press, and disconnect.
    The connected DualSense Edge was detected; physical button acceptance still needs manual device testing.
- Both real executables passed isolated create/copy/reopen/erase verification and an authored live input replay.
    The live sequence entered logs at frame 426 and persisted the chosen name through the running title/menu loop.
- Keyboard defaults are preserved; gamepad/DualSense handling is now authorized and integrated.
- Continue, complete character/party/inventory/story/location initialization, opening gameplay, full save migrations,
    and backup recovery UI remain unfinished. Current persisted records are adventure profiles, not completed game saves.

## Adventure Window Fidelity Correction

- Debug and Release passed 11,016 assertions across 51 native cases. The supported Release workflow finished with
    zero build warnings/errors; both actual executables passed the three menu pixel comparisons and live input/save gate.
- Replaced the full-screen page panel with ROM-positioned opaque windows in parent-to-child order.
- Command ordering, slot selector placement, keyboard/name frames, and small Yes/No placement use reviewed ASM
    records and their actual frame/cursor consumers.
- The command, empty-slot, and empty name-entry pages match their extracted reference images pixel-for-pixel.
- Cancel/No restores the parent selection; name-entry B retains its delete behavior.
- Message speed now displays Fast/Slow and identifies 8 as manual. The battle consumer confirms 1 is fastest and
    7 is slowest automatic, with 15-54-frame delays; the native dialogue/battle implementation remains unfinished.
- Full-game state, Chapter 1 initialization/opening, and Continue remain the next playable slice, not completed work.

## First Gameplay Slice

The Chapter 1 opening-route milestone now implements title/log creation and Continue, the original opening messages,
Burland castle/town/world movement, all loaded daytime route actors and their dialogue/effect programs, inventory/
equipment/derived stats, merchant buy/sell, inn/healing, one-time treasure, world encounters, physical battle rewards
and character level curves through 99. Native gameplay state is persisted in version 2; version-1 profiles remain readable.

Verification: Debug and Release each passed 11,253 assertions across 65 native cases. The full supported Release workflow
finished with zero build warnings/errors. Both executables passed the input-driven gameplay journey and menu/save gates;
the journey reached world-map play, earned 12 EXP and 81 gold, advanced Ragnar to level 2 and preserved state on reload.
The original title checkpoints and three menu captures retain their exact pixel comparisons.

The supported bounded gameplay gate verifies all 71 loaded actor dialogue branches and choice programs, normal-input
castle/town/world traversal, full-state reload, authentic nonblank world/battle rendering and real battle money/EXP
rewards reaching level 2 (12 EXP). This is not completion of the whole port or all battle/monster-AI semantics.
Remaining work includes complete Chapter 1 story progression beyond the opening route, broader monster/spell/AI/
status behavior, other chapters, night-cycle actor variants, complete combat snapshots, vehicles and backup recovery UI.

The first implementation slice should be complete from title screen through new-game creation into the opening playable
scene, including name entry, save creation, input, rendering, dialogue, and the relevant event state. Each behavior must
cite reference evidence and gain a deterministic test before the next gameplay slice begins.
