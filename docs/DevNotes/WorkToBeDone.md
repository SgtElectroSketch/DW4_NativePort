# Work to Be Done

This is the ordered implementation backlog for the native C++ reimplementation. Work should proceed as complete,
testable vertical slices rather than as a bank-by-bank translation of the 6502 program.

Related design documents:

- [Native architecture](../port/ARCHITECTURE.md)
- [Behavior reconstruction rules](../port/BEHAVIOR_NOTES.md)
- [Current port status](../port/PORTING_STATUS.md)
- [Routine-to-native traceability](../port/routine-map.tsv)

## Non-Negotiable Rules

- [ ] AI IS NEVER TO GENERATE CODE BEYOND THE INITIAL SETUP!!!!
  - [ ] Use AI only for explanation, research guidance, documentation review, and critique unless this rule is explicitly changed by the project owner.
  - [ ] Write all new production and test code manually after understanding the relevant design and evidence.
  - [ ] Record any permitted AI assistance accurately in `AI_UseTracking.md`.
- [ ] NEVER APPROVE OR MERGE AI-GENERATED CODE BEYOND THE INITIAL SETUP!!!!
  - [ ] Review every diff before staging or committing it.
  - [ ] Reject generated code even when it compiles if the project owner cannot explain and maintain it.
  - [ ] Keep human review responsible for correctness, architecture, licensing, and security.
- [ ] Never call, link, interpret, or otherwise execute the original assembly from the native program.
  - [ ] Keep every production project reference inside `native/` or approved platform dependencies.
  - [ ] Reject `.asm`, `.s`, ROM, emulator, 6502 interpreter, and reference-tool runtime inputs.
  - [ ] Run the native-boundary check before accepting each slice.
- [ ] Keep `DW4.Game` deterministic and independent of SDL, filesystems, rendering, audio, and reference tools.
  - [ ] Pass explicit input and content values into game logic rather than reading platform state from inside it.
  - [ ] Keep clocks, random seeds, file paths, devices, and SDL handles outside `DW4.Game`.
  - [ ] Prove determinism with repeated-input tests and state snapshots.
- [ ] Use the disassembly, routine contracts, traces, and extracted data as evidence, not as the native architecture.
  - [ ] Describe observable behavior before choosing C++ classes or modules.
  - [ ] Group native code by gameplay responsibility rather than PRG bank or RAM address.
  - [ ] Cite evidence without reproducing the NES memory map as global mutable state.
- [ ] Preserve player-observable behavior and data relationships; binary-identical 6502 output is not a goal.
  - [ ] Match rules, outcomes, ordering, and meaningful timing observed by the player.
  - [ ] Permit native representations and platform APIs when they preserve those observations.
  - [ ] Document intentional deviations instead of quietly changing acceptance criteria.
- [ ] Do not commit ROMs, extracted assets, save files, emulator captures, or packaged copies of those files.
  - [ ] Keep all extraction, runtime-asset, capture, save, staging, and package outputs under ignored paths.
  - [ ] Run the copyright-boundary check before every commit and release package.
  - [ ] Inspect staged files manually when adding new extensions or asset directories.
- [ ] Do not weaken or bypass the native-boundary or copyright-boundary checks to make a build pass.
  - [ ] Fix the violating path, dependency, or artifact at its source.
  - [ ] Treat changes to boundary scripts and ignore rules as architecture/security changes requiring explicit review.
  - [ ] Add a focused regression check whenever a boundary defect is discovered.
- [ ] Do not add placeholder gameplay, inert controls, temporary art, or fabricated data to claim progress.
  - [ ] Expose only commands and controls whose complete behavior exists.
  - [ ] Use authentic locally extracted assets or clearly authored test fixtures, never fake runtime content.
  - [ ] Mark incomplete work in the backlog rather than hiding it behind a stubbed user experience.
- [ ] Finish and validate one vertical slice before opening the next one.
  - [ ] Define the slice's entry state, player actions, exit state, and acceptance evidence.
  - [ ] Complete rules, content, presentation, persistence, errors, and tests needed by that flow.
  - [ ] Update status and traceability before beginning adjacent gameplay work.

## Definition of Done for Every Gameplay Slice

- [ ] Identify the controlling reference routines, data ranges, contracts, and runtime observations.
  - [ ] Start from the visible behavior or failing parity checkpoint.
  - [ ] Record bank/address labels, callers, data tables, and trace evidence that directly control it.
  - [ ] Separate verified facts from hypotheses that still need a discriminating capture or test.
- [ ] Write the player-observable behavior contract before implementing it.
  - [ ] State inputs, preconditions, state changes, outputs, timing, and failure behavior in platform-neutral terms.
  - [ ] Include edge cases, ordering rules, RNG consumption, and persistence effects.
  - [ ] Define what evidence will prove the contract correct.
- [ ] Add the native mapping and evidence citation to `docs/port/routine-map.tsv`.
  - [ ] Name the owning native symbol or subsystem.
  - [ ] Cite the most direct reference routine, data, trace, or capture.
  - [ ] Describe whether the mapping is infrastructure, partial, implemented, or intentionally adapted.
- [ ] Load required game data through typed `DW4.Content` records; do not parse JSON in `DW4.Game`.
  - [ ] Define domain-specific identifiers and immutable records.
  - [ ] Parse and validate files once at the content boundary.
  - [ ] Pass typed records or read-only repositories into game logic.
- [ ] Implement deterministic game rules in `DW4.Game` and platform presentation in `DW4.Desktop`.
  - [ ] Keep the rule result independent of frame render rate, device type, and filesystem layout.
  - [ ] Convert game output into render/audio/input/save service calls at the desktop boundary.
  - [ ] Avoid duplicating game rules in presentation code.
- [ ] Add unit tests for rules and an integration or replay test for the complete interaction.
  - [ ] Cover normal behavior, boundary values, invalid inputs, and RNG-sensitive branches.
  - [ ] Use authored fixtures that do not contain copyrighted extracted payloads.
  - [ ] Verify the player-visible interaction from its initial state through completion.
- [ ] Compare native results against reference traces or captures at explicit checkpoints.
  - [ ] Choose checkpoints before implementation so they cannot be selected to hide divergence.
  - [ ] Compare every relevant state field, input frame, output event, and meaningful presentation frame.
  - [ ] Report the first unexplained difference with enough context to reproduce it.
- [ ] Exercise keyboard and controller paths, save/load where applicable, and error handling.
  - [ ] Test equivalent actions through every supported input device.
  - [ ] Restart the process and verify persisted state instead of testing only in-memory state.
  - [ ] Trigger missing, invalid, disconnected, and interrupted resource conditions deliberately.
- [ ] Update `docs/port/PORTING_STATUS.md` without narrowing the remaining scope.
  - [ ] Move only demonstrably complete behavior into the implemented list.
  - [ ] Preserve all unfinished requirements and add newly discovered work explicitly.
  - [ ] State residual limitations of the completed slice.
- [ ] Pass `test-native.cmd`, the native-boundary check, and the copyright-boundary check.
  - [ ] Run from a clean working directory with the intended configuration.
  - [ ] Require zero new warnings and zero failed assertions.
  - [ ] Review staged content after all checks pass.

## Phase 0 - Repository and Toolchain Foundation

- [x] Move the existing disassembly, analysis, tools, and documentation intact under `reference/`.
  - [x] Preserve each tracked file byte-for-byte at its new relative location.
  - [x] Update the cc65 submodule path without changing its pinned commit.
  - [x] Keep reference-relative scripts functional after the move.
- [x] Build the moved reference solution and reproduce the exact expected ROM SHA-256.
  - [x] Build the reference solution from `reference/`.
  - [x] Produce the expected 524,304-byte NES image.
  - [x] Verify SHA-256 `373BE958CB33651FE599A6B282D2A232EB3B99559C258B2C70B53DF0FA31E34A`.
- [x] Create `DragonWarrior4.Native.sln` with `DW4.Game`, `DW4.Content`, `DW4.Desktop`, and `DW4.Tests`.
  - [x] Configure Debug and Release x64 mappings for every project.
  - [x] Set Game and Content as static libraries and Desktop and Tests as applications.
  - [x] Add Visual Studio filters matching `src/` and `include/` organization.
- [x] Configure C++23, x64 Debug/Release, strict warnings, SDL3, SDL3_image, nlohmann-json, and Catch2.
  - [x] Pin vcpkg to an immutable registry baseline.
  - [x] Enable warning level 4, warnings as errors, SDL checks, and conforming C++ mode.
  - [x] Restore and link all declared dependencies through shared project properties.
- [x] Add deterministic frame/input primitives at the NES NTSC cadence.
  - [x] Define the 256 by 240 logical dimensions and 60.0988-Hz cadence.
  - [x] Represent all eight NES buttons in one explicit bitmask.
  - [x] Advance exactly one logical frame for each `Game::tick` call.
- [x] Add native project checks that reject assembly inputs and dependencies on the reference implementation.
  - [x] Scan every native/test project for assembly build items and source extensions.
  - [x] Reject project references that escape native/test ownership.
  - [x] Reject reference projects in the native solution.
- [x] Add copyright checks for ROMs, extracted assets, captures, archives, and packaged copies.
  - [x] Ignore known ROM, asset, capture, save, archive, build, and staging paths.
  - [x] Audit tracked and visible untracked files before native builds.
  - [x] Permit only the explicit documentation-screenshot exception.
- [x] Add `build-native.cmd`, `test-native.cmd`, and `extract-native-assets.cmd`.
  - [x] Locate MSBuild through Visual Studio discovery.
  - [x] Build before running tests.
  - [x] Require ignored extraction output before rebuilding and extracting assets.
- [x] Verify the asset extractor can consume the exact rebuilt ROM and write to an ignored directory.
  - [x] Run a bounded text-stage extraction smoke test.
  - [x] Reject an unignored in-repository output directory.
  - [x] Confirm generated assets remain invisible to normal Git staging.
- [x] Add an initial game unit test and content integration tests.
  - [x] Test default frame state, one deterministic tick, and button-mask queries.
  - [x] Test all required top-level catalog families using authored temporary JSON fixtures.
  - [x] Test rejection of incomplete extraction output.

## Phase 1 - Evidence, Content, and Parity Baseline

Complete this before implementing gameplay so later behavior can be measured rather than judged by appearance alone.

### Asset Management

- [x] Run every extraction stage and inventory the resulting text, maps, sprites, monsters, graphics, screens, and audio.
  - [x] Run the complete extractor against the exact rebuilt ROM.
  - [x] Confirm every stage produced its expected root catalog and payload directories.
  - [x] Review the asset families and their metadata rather than assuming PNG/WAV previews are complete runtime models.
- [x] Verify every generated file remains ignored, including runtime/package copies created during development.
  - [x] Test canonical generated, extracted, runtime, build, and package path patterns with `git check-ignore`.
  - [x] Scan visible untracked files for prohibited payload extensions and directory patterns.
  - [x] Run the copyright-boundary script successfully.
- [ ] Document which outputs are runtime inputs and which are reference-only evidence.
  - [ ] List every extractor stage and each file type it emits.
  - [ ] Mark typed metadata required by gameplay separately from rendered previews and diagnostic contact sheets.
  - [ ] Classify screenshots and VGM traces as parity evidence unless a runtime design explicitly consumes them.
  - [ ] Record the owning loader and consumer planned for every runtime input.
- [ ] Treat captured screens as comparison evidence, not as background images for the native game.
  - [ ] Render title, map, menu, and battle scenes from structured data and native state.
  - [ ] Keep captured reference images under ignored evidence paths.
  - [ ] Use captures only at documented parity checkpoints and in comparison tooling.
- [ ] Define versioned, authored schemas for each JSON document consumed by the native runtime.
  - [ ] Inventory every field currently read or planned by the native loaders.
  - [ ] Define required, optional, nullable, identifier, numeric-range, and array-cardinality rules.
  - [ ] Add an explicit schema version and compatibility policy.
  - [ ] Store authored schemas without embedding extracted game payloads.
  - [ ] Create valid and invalid authored examples for schema tests.
- [ ] Validate required files, schema versions, identifiers, dimensions, palette indexes, and cross-references at startup.
  - [ ] Validate the complete file manifest before entering the title scene.
  - [ ] Reject unknown schema versions with an actionable re-extraction message.
  - [ ] Enforce unique IDs and valid references between maps, tilesets, monsters, names, sprites, and audio.
  - [ ] Enforce image dimensions, frame counts, palette sizes, and numeric bounds.
  - [ ] Report all independent validation errors in one pass when practical.
- [ ] Produce clear diagnostics for missing, stale, malformed, or partially extracted asset sets.
  - [ ] Distinguish file-not-found, permission, JSON syntax, schema, cross-reference, and version failures.
  - [ ] Include the failing relative path, field, expected form, and actual value.
  - [ ] Explain how to run the extraction command that repairs the failure.
  - [ ] Ensure diagnostics never print ROM bytes or other copyrighted payloads unnecessarily.
- [ ] Convert JSON documents into immutable typed records in `DW4.Content`.
  - [ ] Define stable IDs and records for each domain before writing loaders.
  - [ ] Parse JSON only inside Content implementation files.
  - [ ] Normalize units and representations once during loading.
  - [ ] Expose read-only lookup by typed ID instead of filenames or JSON keys.
  - [ ] Test object lifetime and reference stability for the duration of a game session.
- [ ] Keep all ROM addresses and extraction-specific details inside the content/evidence boundary.
  - [ ] Prevent bank/address strings from entering game-domain public interfaces.
  - [ ] Retain source citations in content metadata or traceability docs for debugging.
  - [ ] Convert extraction representations into domain values before handing them to `DW4.Game`.
- [ ] Add integration tests using small authored fixtures rather than committed extracted payloads.
  - [ ] Cover one minimal valid document for every schema.
  - [ ] Cover missing fields, invalid types, duplicate IDs, bad references, bounds, and unsupported versions.
  - [ ] Generate temporary files during the test and delete them through RAII cleanup.
  - [ ] Keep fixture text, names, maps, and statistics invented rather than copied from DW4.
- [x] Decide how installed builds locate user-extracted assets without copying them into Git or release archives.
  - [x] Use `native/assets/generated` as the development default relative to the repository root.
  - [x] Support `--assets <directory>` for an explicit local asset root.
  - [x] Keep actual extracted payloads out of tracked source and release archives.

### Reference Capture and Replay

- [ ] Define a versioned replay format containing initial state, RNG state, and one controller sample per game frame.
  - [ ] Specify a file header, format version, game/content compatibility identifiers, and endianness.
  - [ ] Define exactly which initial state is embedded versus loaded by stable ID.
  - [ ] Store one normalized input mask for every simulated frame.
  - [ ] Include RNG state and any external decisions that affect determinism.
  - [ ] Define corruption checks, maximum lengths, and forward-compatibility behavior.
- [ ] Define stable native snapshots for flags, party, inventory, location, field position, battle state, and RNG.
  - [ ] List every deterministic state domain and assign stable field names and types.
  - [ ] Exclude pointers, platform handles, caches, and presentation-only data.
  - [ ] Sort unordered collections before serialization or comparison.
  - [ ] Define how absent/inactive scene-specific state is represented.
- [ ] Add a reference capture workflow for the same checkpoints using existing FCEUX traces and save evidence.
  - [ ] Choose reproducible starting saves or power-on states.
  - [ ] Define input scripts and exact frame checkpoints.
  - [ ] Capture only state fields supported by reference evidence.
  - [ ] Keep ROM, save, screenshot, and trace payloads ignored while tracking authored capture instructions.
- [ ] Add comparison tooling that reports the first divergent frame and field instead of only pass/fail.
  - [ ] Compare replay metadata before frame data.
  - [ ] Stop at or prominently identify the earliest frame mismatch.
  - [ ] Print the state path, expected value, actual value, and nearby input history.
  - [ ] Support a bounded context window before and after divergence.
- [ ] Capture the title/new-game/opening sequence needed by the first vertical slice.
  - [ ] Record power-on through stable title presentation.
  - [ ] Record adventure-log selection and name-entry input paths.
  - [ ] Record new-game initialization through first controllable frame.
  - [ ] Capture checkpoints for state, rendered frame, and selected audio events.
- [ ] Keep captures containing copyrighted screen/audio/save payloads ignored; track only authored expectations and tooling.
  - [ ] Add every capture output directory and extension to ignore rules before generating data.
  - [ ] Track hashes, checkpoint descriptions, and comparison tolerances rather than payloads.
  - [ ] Run the copyright guard after each new capture workflow is introduced.

### Deterministic Core

- [ ] Identify and implement the game's random-number behavior with explicit state ownership.
  - [ ] Locate every relevant RNG routine, seed source, update rule, and caller in the reference evidence.
  - [ ] Determine value width, overflow behavior, output transformation, and call ordering.
  - [ ] Define a native RNG state type owned by deterministic game state.
  - [ ] Require explicit RNG access from rules that consume randomness.
  - [ ] Test known sequences, save/load continuation, and replay continuation.
- [ ] Separate simulation ticks from rendering cadence and wall-clock time.
  - [ ] Treat one `Game::tick` call as one deterministic simulation frame.
  - [ ] Keep elapsed-time accumulation and catch-up policy in Desktop.
  - [ ] Generate rendering from current game state without mutating rules.
  - [ ] Test zero, one, and multiple simulation ticks per presented frame.
- [ ] Define scene/state transitions without recreating the NES RAM map.
  - [ ] Enumerate title, log, name-entry, field, menu, battle, event, and ending scene responsibilities.
  - [ ] Define typed transition requests and required payloads.
  - [ ] Specify which state persists across scenes and which is created/destroyed.
  - [ ] Reject hidden transitions driven by unrelated global byte offsets.
- [ ] Add serialization-independent snapshots for tests and replay comparison.
  - [ ] Build snapshots from public deterministic values rather than save-file bytes.
  - [ ] Use stable field order and typed IDs.
  - [ ] Keep snapshots independent from memory layout and compiler padding.
  - [ ] Add readable structured comparison output.
- [ ] Verify repeated runs with the same initial state and input replay produce identical snapshots.
  - [ ] Run the same replay at least twice in one executable invocation.
  - [ ] Run it in separate processes and Debug/Release configurations.
  - [ ] Compare every checkpoint and final state byte-for-byte in the snapshot format.
  - [ ] Investigate any dependency on clock, iteration order, uninitialized memory, or platform input.

## Phase 2 - Platform Services Required by the First Slice

Implement these capabilities through the first gameplay slice. Do not build unused general-purpose engine features.

### Graphics Rendering

- [ ] Create a 256 by 240 logical framebuffer with integer scaling, pillar/letterboxing, and resize handling.
  - [ ] Understand the difference between logical game pixels and physical monitor pixels.
  - [ ] Decide whether the logical framebuffer is an SDL texture, a CPU-owned pixel buffer uploaded to a texture, or a renderer target.
  - [ ] Define the framebuffer pixel format and document byte order, alpha behavior, and color conversion.
  - [ ] Allocate exactly 256 columns by 240 rows and clear every pixel deterministically before each frame is drawn.
  - [ ] Configure nearest-neighbor sampling so scaled pixel art remains sharp rather than blurred.
  - [ ] Compute the largest whole-number scale that fits inside the current drawable window area.
  - [ ] Center the scaled image and fill unused horizontal or vertical space with a deliberate letterbox color.
  - [ ] Distinguish window size from drawable pixel size so high-DPI display scaling does not distort the image.
  - [ ] Recompute viewport and scale when the window is resized, moved between monitors, maximized, restored, or switched to fullscreen.
  - [ ] Define behavior when the window is smaller than 256 by 240, including whether to allow fractional downscaling or enforce a minimum size.
  - [ ] Verify 1x, 2x, 3x, and 4x scaling with screenshots that show equal-sized source pixels and no cropped rows or columns.
  - [ ] Verify common aspect ratios such as 4:3, 16:9, 16:10, and ultrawide without stretching the logical image.
  - [ ] Verify the top-left and bottom-right logical pixels land at the expected physical coordinates after every resize.
- [ ] Establish explicit draw ordering for backgrounds, windows, text, sprites, fades, and overlays.
  - [ ] List every visual category needed by the first slice and identify which categories may overlap.
  - [ ] Define a stable layer order for cleared background, map or scene background, field or battle objects, windows, text, cursors, fades, and diagnostics.
  - [ ] Decide whether ordering is represented by separate command lists, numeric layers, or fixed renderer passes.
  - [ ] Define ordering within one layer so results do not depend on container iteration order or memory addresses.
  - [ ] Specify how sprite priority, window occlusion, and elements that intentionally appear behind scenery are represented.
  - [ ] Specify whether fades cover the whole scene, exclude selected UI, or operate between scene and overlay layers.
  - [ ] Keep debug overlays in a separate final layer that can be disabled without affecting game rendering.
  - [ ] Create an authored overlap fixture with one item from every layer and record the expected final ordering.
  - [ ] Test that adding an unrelated item to one layer cannot reorder items in another layer.
- [ ] Load indexed PNGs without losing palette or transparency semantics.
  - [ ] Learn how indexed PNGs store palette entries, per-pixel indexes, and optional transparency separately from RGBA PNGs.
  - [ ] Identify which extracted asset families are indexed PNGs and document their expected bit depth, dimensions, and transparent indexes.
  - [ ] Decide whether indexed pixels remain indexed in memory or are converted once into renderer-ready RGBA textures.
  - [ ] Preserve the original palette index alongside rendered color when runtime palette swaps require recoloring without re-decoding the image.
  - [ ] Reject PNGs with unexpected dimensions, color type, bit depth, missing palette data, or out-of-range indexes.
  - [ ] Treat transparent palette entries as transparent rather than as visible black background pixels.
  - [ ] Disable linear filtering and texture mipmaps for pixel-art assets.
  - [ ] Define texture ownership, cache keys, duplicate-load behavior, and cleanup when the content system shuts down.
  - [ ] Test a fixture containing every palette index, transparent pixels, edge pixels, and repeated colors that use different indexes.
  - [ ] Compare decoded pixels against the extractor metadata rather than relying only on visual inspection.
- [ ] Implement NES palette conversion and runtime palette selection from extracted metadata.
  - [ ] Document the NES color-index range, the chosen reference RGB palette, and why NES colors do not map to one universally exact RGB value.
  - [ ] Define typed background and sprite palette records instead of passing raw nested JSON arrays into rendering code.
  - [ ] Validate every palette color index before using it to access the reference RGB palette.
  - [ ] Preserve the four subpalettes, four colors per subpalette, and the shared-background or transparent-color rules where applicable.
  - [ ] Distinguish background palette selection from sprite palette selection.
  - [ ] Apply day, night, map, battle, event, and story-driven palette choices from game state rather than from hardcoded filenames.
  - [ ] Support runtime palette replacement without rebuilding geometry or mutating source asset data.
  - [ ] Define how universal background color and sprite transparency are represented after RGB conversion.
  - [ ] Test known palette records against extractor output and captured reference screens.
  - [ ] Test invalid palette indexes and incomplete palette records with clear startup diagnostics.
- [ ] Implement 8 by 8 tiles, 16 by 16 metatiles, sprite frames, horizontal/vertical flips, and clipping.
  - [ ] Define integer types for tile indexes, palette selections, source rectangles, logical positions, and flip flags.
  - [ ] Render one 8 by 8 tile at an exact logical coordinate with no scaling or filtering artifacts inside the logical framebuffer.
  - [ ] Compose one 16 by 16 metatile from its four ordered 8 by 8 child tiles.
  - [ ] Confirm metatile quadrant order against extracted tileset metadata and reference captures.
  - [ ] Render field-sprite frames using their documented frame order, dimensions, transparent indexes, and palette selection.
  - [ ] Implement horizontal flip, vertical flip, and combined flip around the tile or sprite's own bounds.
  - [ ] Clip partially visible graphics against all four logical framebuffer edges without reading outside source pixels.
  - [ ] Define behavior for negative logical positions and objects extending past 255 or 239.
  - [ ] Keep collision and gameplay coordinates independent from interpolated or decorative rendering positions.
  - [ ] Test all four flip combinations with an asymmetric authored tile so incorrect orientation is obvious.
  - [ ] Test clipping with one-pixel visibility at each edge and with objects entirely outside the viewport.
  - [ ] Test shared source tiles rendered simultaneously with different palettes and flip flags.
- [ ] Implement the main and alternate fonts with the original text-code mapping.
  - [ ] Inventory every extracted main-font and alternate-font glyph, its tile index, and its documented text-code meaning.
  - [ ] Define typed glyph identifiers and keep raw text command bytes separate from printable glyph codes.
  - [ ] Map digits, uppercase letters, lowercase letters, punctuation, arrows, and special symbols explicitly.
  - [ ] Define a visible fallback glyph and diagnostic for an unknown printable code rather than silently dropping it.
  - [ ] Load the alternate font only for the scenes or modes that selected it in the reference game.
  - [ ] Preserve fixed glyph dimensions and pixel alignment when drawing text.
  - [ ] Define whether spaces advance the cursor without drawing and how punctuation affects width.
  - [ ] Render a complete authored glyph chart for both fonts and label each cell with its code.
  - [ ] Compare known title, menu, dialogue, and credits strings against reference captures.
  - [ ] Keep dialogue control commands out of the glyph mapper so they cannot accidentally render as `<XX>` text.
- [ ] Implement reusable menu/window borders, cursors, numeric fields, and text layout.
  - [ ] Identify every border tile and corner variant used by the first slice.
  - [ ] Define a window rectangle in logical tile or pixel coordinates with explicit interior and border dimensions.
  - [ ] Build borders from corner, horizontal-edge, vertical-edge, and fill tiles without stretching pixel art.
  - [ ] Reject or handle windows too small to contain all required border pieces.
  - [ ] Define cursor graphics, animation frames, hotspot position, and selection-row alignment.
  - [ ] Implement left-, right-, and center-aligned numeric fields with explicit width and overflow behavior.
  - [ ] Decide how leading spaces, leading zeroes, signs, currency units, and values larger than the field are displayed.
  - [ ] Implement line measurement, word wrapping, explicit line breaks, page breaks, and maximum visible rows.
  - [ ] Preserve text command semantics rather than rendering extractor preview markers literally.
  - [ ] Separate reusable layout calculations from SDL draw calls so layout can be unit tested without a GPU.
  - [ ] Test minimum and maximum window sizes, empty text, longest known words, maximum numeric values, and cursor movement at list boundaries.
  - [ ] Compare title menus, status windows, shops, battle commands, and dialogue windows against reference captures as each becomes reachable.
- [ ] Implement fade-in, fade-out, blanking, and scene transitions required by the title/opening flow.
  - [ ] Catalog every transition used from process start through first player control and record its direction, duration, and trigger.
  - [ ] Decide whether fades interpolate palette colors, apply a full-screen color overlay, or reproduce another reference-specific method.
  - [ ] Represent transition progress in deterministic simulation ticks rather than wall-clock-only animation state.
  - [ ] Define when input is accepted, buffered, or ignored during each transition.
  - [ ] Define whether audio starts, stops, or continues before, during, or after the visual transition.
  - [ ] Implement immediate blanking for scenes that disable rendering rather than fading.
  - [ ] Prevent one-frame flashes of an old or uninitialized scene at transition boundaries.
  - [ ] Specify ownership of outgoing and incoming scene state while a transition is active.
  - [ ] Test the first frame, midpoint, final frame, and one frame after completion for every transition type.
  - [ ] Compare transition duration and visible endpoints against captured reference frames.
- [ ] Add render-command tests that do not require a GPU.
  - [ ] Define plain-data render commands for operations such as clear, tile, sprite, text, window, fade, and overlay.
  - [ ] Keep render-command generation in deterministic code that does not construct an SDL window or renderer.
  - [ ] Write authored fixtures with known game state and expected ordered command sequences.
  - [ ] Test coordinates, source rectangles, palette identifiers, layer order, clipping rectangles, and flip flags.
  - [ ] Test empty scenes and scenes containing the maximum expected number of commands.
  - [ ] Produce readable failure output that identifies the first differing command and field.
  - [ ] Ensure tests compare semantic commands rather than unstable pointers, allocation order, or platform handles.
  - [ ] Run these tests in Debug and Release configurations without requiring a display or GPU driver.
- [ ] Add screenshot comparison with documented tolerances for the first slice.
  - [ ] Select exact reference checkpoints and record the scene, frame, input history, and state needed to reproduce each image.
  - [ ] Store copyrighted reference screenshots only in ignored local evidence directories.
  - [ ] Define how the test locates local reference images without embedding machine-specific absolute paths.
  - [ ] Capture native images from the 256 by 240 logical framebuffer before physical display scaling.
  - [ ] Decide whether comparison is exact indexed-pixel equality, exact RGBA equality, or a documented tolerance for palette conversion.
  - [ ] Define allowed and forbidden differences for colors, alpha, positions, and timing.
  - [ ] Generate a visual diff image and numeric summary when comparison fails.
  - [ ] Mask only explicitly documented nondeterministic or intentionally adapted regions; never use broad masks to hide defects.
  - [ ] Test the comparison tool with identical, one-pixel-shifted, one-color-changed, and wrong-sized images.
  - [ ] Require unexplained differences to be resolved or documented before accepting the slice.

### Input Handling

- [ ] Represent current, pressed, released, and repeated button states per simulation frame.
  - [ ] Define one normalized eight-button mask independent of keyboard or gamepad APIs.
  - [ ] Derive pressed edges from current buttons absent in the previous frame.
  - [ ] Derive released edges from previous buttons absent in the current frame.
  - [ ] Define deterministic repeat delay and repeat interval counters per direction or command.
  - [ ] Reset transition state predictably after focus loss, device change, and scene initialization.
- [ ] Map keyboard and SDL gamepads to A, B, Start, Select, and the directional pad.
  - [ ] Choose documented default keyboard keys and gamepad buttons.
  - [ ] Normalize analog stick thresholds and D-pad input into digital directions.
  - [ ] Define behavior when keyboard and gamepad inputs occur simultaneously.
  - [ ] Prevent platform key repeat events from bypassing native repeat rules.
  - [ ] Test every physical binding produces exactly the intended logical bit.
- [ ] Implement the reference menu-repeat timing and opposing-direction behavior.
  - [ ] Capture initial delay and subsequent repeat cadence from reference traces.
  - [ ] Determine behavior for Up+Down and Left+Right held simultaneously.
  - [ ] Separate menu repeat from field movement and text-input repeat when evidence differs.
  - [ ] Test press, hold, release, direction change, and simultaneous-direction sequences frame by frame.
- [ ] Handle controller connection/disconnection without changing deterministic game state.
  - [ ] Listen for SDL gamepad-added and gamepad-removed events in Desktop.
  - [ ] Assign a stable active-device policy without storing SDL handles in `DW4.Game`.
  - [ ] Clear stuck held-state bits when a device disappears.
  - [ ] Decide whether gameplay pauses, falls back to keyboard, or waits for reconnection.
  - [ ] Test disconnect during movement, menus, dialogue, and battle selection.
- [ ] Add configurable bindings only after the original control path is complete.
  - [ ] Define command IDs independently from SDL scancodes and gamepad constants.
  - [ ] Provide a capture flow for rebinding and a way to cancel it.
  - [ ] Detect conflicts, reserved keys, and inaccessible configurations.
  - [ ] Persist bindings separately from gameplay saves and provide reset-to-default.
- [ ] Test keyboard, controller, focus loss, and held-button transitions.
  - [ ] Unit test normalized masks and edge calculations with authored frame sequences.
  - [ ] Integration test SDL event translation with supported device types where possible.
  - [ ] Verify focus loss cannot leave movement or confirmation stuck active.
  - [ ] Verify deterministic replays consume normalized input, not live devices.

### Audio Playback

- [ ] Load extracted WAV data and loop metadata through `DW4.Content`.
  - [ ] Define typed track/effect IDs, sample format, channel count, sample rate, duration, and optional loop range.
  - [ ] Validate WAV headers, metadata JSON, referenced filenames, and loop sample bounds.
  - [ ] Keep decoded sample ownership in Content or an explicit audio-asset cache.
  - [ ] Report missing or unsupported audio without exposing absolute developer assumptions.
- [ ] Implement SDL3 audio-device ownership and format conversion.
  - [ ] Enumerate the desired internal mix format and SDL device negotiation requirements.
  - [ ] Open the output device through an RAII owner with clean shutdown.
  - [ ] Convert source rate, channel layout, and sample type into the internal mix format.
  - [ ] Define thread-safe communication between deterministic game audio events and the audio callback/stream.
  - [ ] Handle device-open failure, hotplug, pause, and recovery with actionable diagnostics.
- [ ] Support music intro/loop boundaries without audible gaps.
  - [ ] Interpret loop start/end in sample frames, not rounded wall-clock seconds.
  - [ ] Play the intro once and wrap exactly to loop start at loop end.
  - [ ] Preserve fractional conversion accuracy when source and device sample rates differ.
  - [ ] Test the samples immediately before and after wrap for missing, duplicated, or discontinuous data.
- [ ] Support concurrent sound effects, interruption, priority, and channel behavior required by the first slice.
  - [ ] Capture which sounds may overlap and which replace an existing instance.
  - [ ] Define a bounded voice pool and behavior when all voices are occupied.
  - [ ] Define priority, restart, duplicate, stop, and scene-transition rules.
  - [ ] Mix without integer overflow or unintended clipping and test representative simultaneous effects.
- [ ] Map game music/SFX identifiers to typed content records instead of filenames in gameplay code.
  - [ ] Define distinct `MusicId` and `SoundEffectId` types or enums.
  - [ ] Resolve IDs to content records at the platform/content boundary.
  - [ ] Emit semantic play/stop/fade events from `DW4.Game` without filesystem paths.
  - [ ] Validate every referenced ID exists at startup.
- [ ] Use VGM captures as fidelity evidence; do not execute the original sound engine at runtime.
  - [ ] Document which VGM track/effect corresponds to each typed ID.
  - [ ] Compare event timing, duration, loop points, and channel activity against native playback.
  - [ ] Keep VGM playback or 6502/APU execution out of the shipped runtime path.
- [ ] Add headless tests for event scheduling, loop points, and music/SFX state transitions.
  - [ ] Test play, stop, replace, restart, fade, and scene-transition event sequences without an audio device.
  - [ ] Test exact loop wrap indexes with tiny authored sample buffers.
  - [ ] Test voice priority and exhaustion deterministically.
  - [ ] Verify the same game replay emits the same ordered audio-event stream.

### Save System Foundation

- [ ] Derive adventure-log semantics, defaults, validation, and copy/delete behavior from save-RAM evidence.
  - [ ] Inventory every persistent field and bit from save-RAM reports and controlling routines.
  - [ ] Determine new-game defaults for each chapter and hero choice.
  - [ ] Trace checksum/validation, empty-slot detection, copy, erase, and load behavior.
  - [ ] Write authored save behavior contracts before defining the native format.
- [ ] Define a versioned native save container separate from in-memory game structures.
  - [ ] Define magic, version, payload length, integrity check, and encoding/endianness.
  - [ ] Serialize stable values and typed IDs rather than object memory or pointers.
  - [ ] Separate slot metadata used by previews from the full game-state payload where useful.
  - [ ] Define compatibility and migration behavior before releasing a version.
- [ ] Store saves in the operating system's per-user data directory, never in tracked source/assets paths.
  - [ ] Determine the Windows per-user application-data path through an appropriate platform API.
  - [ ] Create an application-specific directory with clear ownership and permissions.
  - [ ] Keep saves separate from executable, source, extracted assets, and configuration files.
  - [ ] Display or log the save location for troubleshooting without treating it as a repository path.
- [ ] Use atomic write/replace behavior and preserve the previous valid save on failure.
  - [ ] Write a complete temporary file in the same filesystem as the destination.
  - [ ] Flush and close the temporary file before replacement.
  - [ ] Validate the temporary save before replacing the current save.
  - [ ] Maintain a backup or rollback path and clean stale temporary files safely.
  - [ ] Test interruption at each stage of the write process.
- [ ] Detect corruption and unsupported versions with a recoverable user-facing error.
  - [ ] Validate header, length, checksum, field ranges, IDs, and required invariants before constructing game state.
  - [ ] Distinguish corrupt, truncated, unsupported-newer, and migration-required saves.
  - [ ] Preserve the bad file for recovery rather than overwriting it automatically.
  - [ ] Return the user to a safe menu with repair, restore, or erase choices.
- [ ] Add deterministic new-game initialization and round-trip tests for every field introduced by the first slice.
  - [ ] Build new-game state from explicit constants/content records rather than zeroing arbitrary memory.
  - [ ] Snapshot every initialized field before saving.
  - [ ] Save, destroy in-memory state, reload, and compare all fields.
  - [ ] Test minimum/maximum names, slot indexes, and first-slice event flags.
- [ ] Defer NES save import/export until native save semantics are complete and tested.
  - [ ] Keep `.sav` and `.srm` files ignored.
  - [ ] Do not shape the internal native model around byte-for-byte NES SRAM layout.
  - [ ] Record import/export as a separate offline-tool decision after the native format stabilizes.

### Desktop Runtime

- [ ] Replace the blank host with explicit scene presentation driven by `DW4.Game` frame output.
  - [ ] Define a read-only render snapshot or ordered render-command output from game state.
  - [ ] Render at least the first complete scene without letting Desktop decide game rules.
  - [ ] Keep interpolation or platform effects separate from deterministic positions.
  - [ ] Verify the blank clear remains only as a fallback/error presentation.
- [ ] Define renderer, audio, input, save, and asset service interfaces at the platform boundary.
  - [ ] List the minimum operations each gameplay slice requires from each service.
  - [ ] Use domain IDs and plain data rather than SDL handles in interfaces visible to Game.
  - [ ] Define ownership, lifetime, failure reporting, and thread requirements.
  - [ ] Provide test doubles or adapters where deterministic tests need to observe outputs.
- [ ] Add clean startup/shutdown and actionable errors for assets, video, audio, and save storage.
  - [ ] Establish initialization order and reverse destruction order for every service.
  - [ ] Stop startup immediately when a required service fails, after cleaning prior services.
  - [ ] Distinguish optional audio/device failures from required content/render failures according to policy.
  - [ ] Include the failed subsystem, operation, and recovery action in user-visible errors.
- [ ] Pause or constrain wall-clock accumulation when the process loses focus or resumes after a stall.
  - [ ] Detect focus/minimize/resume events in Desktop.
  - [ ] Decide which states pause automatically and whether audio also pauses.
  - [ ] Reset or clamp accumulated elapsed time so a debugger break cannot trigger thousands of catch-up ticks.
  - [ ] Test focus loss before, during, and after input-sensitive scenes.
- [ ] Add basic runtime options for integer scale, fullscreen/windowed mode, volume, and input bindings when functional.
  - [ ] Define a versioned configuration format separate from saves.
  - [ ] Validate values and fall back safely when monitors/devices/settings change.
  - [ ] Apply display and volume changes without corrupting deterministic game state.
  - [ ] Provide explicit apply/cancel/default behavior and persist only confirmed values.

## Phase 3 - First Complete Vertical Slice

Target: launch the game, reproduce the title/adventure-log flow, create a new game, and enter the first controllable
opening scene with correct rendering, input, audio, dialogue, event flags, and persistence.

### Title and Adventure Log

- [ ] Reproduce title-screen composition, animation, timing, music, and accepted inputs.
  - [ ] Capture the title scene from power-on and identify every stable and animated visual element.
  - [ ] Identify the assets, palettes, positions, frame durations, and draw order used by the title.
  - [ ] Record exactly when music begins and which inputs are ignored, accepted, or debounce-delayed.
  - [ ] Build a deterministic title-scene state machine driven by simulation frames.
  - [ ] Compare key native frames and input transitions against the captured reference sequence.
- [ ] Implement every command exposed by the title/adventure-log UI; do not ship inert menu entries.
  - [ ] Inventory every visible command and the conditions that enable, disable, hide, or rename it.
  - [ ] Define the destination scene and required state for each command.
  - [ ] Implement confirmation, cancellation, unavailable-command feedback, and return paths.
  - [ ] Test every command with empty, partially occupied, and fully occupied save slots.
- [ ] Implement empty/occupied slot presentation from actual native save state.
  - [ ] Define the metadata needed to display an empty slot and an occupied adventure log.
  - [ ] Load slot previews without constructing a complete active game session.
  - [ ] Display chapter, hero name, level, location, and other evidenced fields accurately.
  - [ ] Handle corrupt or unsupported slots distinctly from empty slots.
  - [ ] Test all slot combinations and longest permitted display values.
- [ ] Implement cursor movement, cancellation, confirmation, transitions, and sound effects.
  - [ ] Record initial cursor positions, wrap/clamp rules, repeat timing, and disabled-option behavior.
  - [ ] Map logical directional, confirm, and cancel actions rather than reading platform keys directly.
  - [ ] Emit cursor, confirm, cancel, and error sound events at the correct state transition.
  - [ ] Prevent repeated confirms from activating multiple transitions.
  - [ ] Replay authored input sequences and compare cursor/state checkpoints frame by frame.
- [ ] Implement save creation, selection, copy/delete, and validation behaviors evidenced by the reference game.
  - [ ] Trace prompts, confirmation order, destination restrictions, and cancellation behavior for each operation.
  - [ ] Prevent copy/delete operations from losing a valid source save on failure.
  - [ ] Validate source and destination slots before mutating files.
  - [ ] Refresh slot metadata only after an operation completes successfully.
  - [ ] Test copy onto occupied/empty slots, delete confirmation/cancel, corrupt sources, and interrupted writes.

### Name Entry and New Game

- [ ] Reproduce the name-entry grid, character rules, cursor behavior, editing, confirmation, and cancellation.
  - [ ] Capture grid dimensions, available symbols, initial cursor, navigation, wrapping, and page changes.
  - [ ] Identify maximum name length, blank-name behavior, duplicate symbols, and forbidden characters.
  - [ ] Implement append, delete/backspace, confirm, cancel, and full-buffer feedback.
  - [ ] Keep displayed cursor and text state derived from deterministic name-entry state.
  - [ ] Test every grid boundary, maximum length, deletion at empty, and cancellation at each stage.
- [ ] Validate name length and text encoding through typed game rules.
  - [ ] Define a native name value type with explicit maximum logical characters.
  - [ ] Map selected glyph IDs to runtime text symbols without storing raw UI pointers.
  - [ ] Reject unsupported symbols and invalid byte sequences at construction/load time.
  - [ ] Test shortest, longest, repeated, punctuation, and round-trip save names.
- [ ] Initialize all chapter, party, inventory, flag, RNG, location, and save fields required for a new game.
  - [ ] Derive every initial value from reference routines, save-RAM evidence, and content records.
  - [ ] Create a named new-game initializer rather than depending on default-zero behavior.
  - [ ] Initialize hero identity, chapter state, party membership, stats, equipment, inventory, money, flags, RNG, map, and position.
  - [ ] Validate cross-field invariants before exposing the new state.
  - [ ] Compare a complete initial snapshot against reference evidence.
- [ ] Persist the new adventure log and verify it can be loaded after process restart.
  - [ ] Save through the atomic slot-writing path immediately after confirmed creation.
  - [ ] Terminate the process or destroy all state so the verification cannot reuse memory.
  - [ ] Reload the slot and compare every first-slice persistent field.
  - [ ] Test creation failure leaves the destination slot empty or previously valid.

### Opening Playable Scene

- [ ] Load and render the correct opening map/submap, palette, entities, and initial camera position.
  - [ ] Identify the chapter's opening map, submap, tileset, day/night state, palette, and entity layout.
  - [ ] Convert extracted records into typed map and entity definitions.
  - [ ] Instantiate mutable scene state separately from immutable definitions.
  - [ ] Compute initial camera bounds and party screen position from map coordinates.
  - [ ] Compare the first controllable frame against reference state and screenshot evidence.
- [ ] Implement player movement, facing, animation, collision, and map boundaries needed in the opening area.
  - [ ] Capture movement cadence, tile-step duration, facing changes, and animation-frame timing.
  - [ ] Separate intended movement from collision resolution and final position.
  - [ ] Classify passable, blocked, directional, interactive, and transition tiles used by the area.
  - [ ] Prevent leaving map bounds or entering occupied/blocked cells.
  - [ ] Test movement into every relevant tile type from every meaningful direction.
- [ ] Implement the opening event sequence and all state changes reachable before leaving the slice boundary.
  - [ ] List every trigger, prerequisite flag, actor action, dialogue line, movement, and resulting flag change.
  - [ ] Define native event steps as typed operations rather than 6502 instruction emulation.
  - [ ] Specify when player input is locked and restored.
  - [ ] Persist event progress safely so reload cannot duplicate rewards or soft-lock progression.
  - [ ] Replay every branch and compare flags, entity layout, position, and dialogue order.
- [ ] Implement dialogue boxes, control codes, paging, choice prompts, and dynamic values used by the opening scene.
  - [ ] Decode each encountered symbol into literal glyph, layout command, prompt, pause, insertion, pluralization, or audio event.
  - [ ] Implement line wrapping and paging from logical glyph widths and window dimensions.
  - [ ] Resolve character names, numbers, items, and choices from typed runtime context.
  - [ ] Preserve pauses and input prompts in deterministic frame state.
  - [ ] Test each encountered control code individually and in the complete opening dialogue sequence.
- [ ] Implement the command/menu interactions that are reachable in the opening scene.
  - [ ] Inventory which field commands are available before leaving the opening boundary.
  - [ ] Implement command availability, selection, target choice, result, cancellation, and messages.
  - [ ] Ensure unavailable commands explain failure without mutating state.
  - [ ] Test each command against empty, valid, and invalid targets or locations.
- [ ] Save, quit, reload, and resume at the correct state and position.
  - [ ] Define safe save points and persistent fields for the opening scene.
  - [ ] Save before, during, and after completed events only where reference/native policy permits.
  - [ ] Reload map, entities, flags, party position, facing, inventory, and RNG consistently.
  - [ ] Verify completed one-time events do not replay after loading.
- [ ] Add a deterministic replay from launch to first player control and compare all defined checkpoints.
  - [ ] Record normalized inputs from process start through the first controllable frame.
  - [ ] Define checkpoints for title, log creation, name confirmation, scene entry, and control handoff.
  - [ ] Compare snapshots, render commands/screens, and audio events at each checkpoint.
  - [ ] Require repeated Debug/Release runs to produce identical deterministic results.

### First-Slice Acceptance

- [ ] Complete the full flow without developer commands, hardcoded skips, placeholder data, or inert controls.
  - [ ] Start from the same public executable and menus a player will use.
  - [ ] Remove temporary shortcuts, forced scene IDs, injected saves, and debug-only progression flags.
  - [ ] Exercise every visible control and confirm it has complete behavior.
  - [ ] Perform an uninterrupted manual playthrough of the slice.
- [ ] Match reference checkpoints for frames where timing matters, game state, selected audio, and presentation.
  - [ ] Freeze the reference checkpoint list before final parity work.
  - [ ] Compare deterministic state and event order exactly where supported.
  - [ ] Compare presentation with documented pixel/color/timing tolerances.
  - [ ] Resolve or explicitly document every remaining difference.
- [ ] Pass a clean-checkout workflow: extract assets locally, build, test, run, create a save, and reload it.
  - [ ] Clone or clean to a state without build, dependency, ROM, asset, or save outputs.
  - [ ] Initialize submodules and dependencies using documented commands.
  - [ ] Build/verify the reference ROM and extract assets to ignored storage.
  - [ ] Build/test native Debug and Release, play the slice, restart, and reload.
  - [ ] Confirm no generated/copyrighted output becomes trackable.
- [ ] Update the status and traceability documents before beginning general field exploration.
  - [ ] Mark only fully accepted behavior as implemented.
  - [ ] Add mappings for every reference behavior used by the slice.
  - [ ] Document deviations, limitations, test commands, and remaining adjacent work.

## Phase 4 - Field Exploration Slice

Target: travel through connected field maps with correct menus, dialogue, events, encounters, and persistence.

### Maps and Movement

- [ ] Decode typed location, world-map, tileset, palette, roof, and entity records from extracted metadata.
  - [ ] Define distinct immutable record types and typed IDs for each data family.
  - [ ] Validate dimensions, indexes, references, and special fields during loading.
  - [ ] Keep source addresses as optional evidence metadata, not gameplay keys.
  - [ ] Test one authored valid and several invalid fixtures per record type.
- [ ] Render maps from cells/metatiles rather than using full-map PNG previews as gameplay backgrounds.
  - [ ] Load map cells and resolve each cell through tileset and palette records.
  - [ ] Render only the visible camera region plus required edge coverage.
  - [ ] Reproduce smoothing, roof-region, border, and event-driven tile changes from structured data.
  - [ ] Compare native composition against extracted preview images and reference captures.
- [ ] Implement camera scrolling, submap transitions, doors, stairs, warps, and world/location transitions.
  - [ ] Define camera position, target, bounds, dead zone, and movement cadence.
  - [ ] Define typed transition destinations including map, submap, coordinates, facing, and transition effect.
  - [ ] Prevent repeated activation while a transition is already pending.
  - [ ] Test each transition in both directions and at map boundaries.
- [ ] Implement terrain collision, directional restrictions, counters, damage tiles, and scripted blockers.
  - [ ] Classify every relevant tile behavior from reference evidence.
  - [ ] Evaluate directional entry/exit rules before committing movement.
  - [ ] Trigger counter interactions, damage, and scripts at the correct movement phase.
  - [ ] Test actor, vehicle, chapter, and flag-dependent passability.
- [ ] Implement roofs/interiors, day/night palette behavior, and animated map tiles.
  - [ ] Reproduce inside/outside roof visibility based on party position and region bits.
  - [ ] Select day/night palettes from deterministic world time and map metadata.
  - [ ] Advance animated tile frames at reference cadence without changing collision identity.
  - [ ] Test transitions while roofs, palette cycles, and animation are active.
- [ ] Implement field sprite selection, direction, step animation, palette changes, rotation, and visibility rules.
  - [ ] Resolve entity sprite IDs to sprite sets and character variants.
  - [ ] Select facing and step frames from deterministic entity movement state.
  - [ ] Apply palette/event overrides and documented rotated variants.
  - [ ] Cull or hide entities according to flags without deleting persistent state incorrectly.

### Dialogue and Field Interaction

- [ ] Implement all dialogue symbols and control codes with reference line wrapping and pagination.
  - [ ] Enumerate command bytes `$40` through `$58` and verify each handler's observable meaning.
  - [ ] Separate literal symbols from commands and command operands in typed tokens.
  - [ ] Implement stops, line changes, prompts, pauses, insertions, pluralization, replay, services, and jingles as evidenced.
  - [ ] Compare representative long messages, punctuation, wrapping, and page boundaries.
- [ ] Implement speaker names, choices, numeric values, item/spell/place names, and context-dependent handlers.
  - [ ] Define a dialogue context containing only values needed by the active message.
  - [ ] Resolve lookup group/index pairs through typed Content records.
  - [ ] Format numbers, names, plural forms, and choices within original layout constraints.
  - [ ] Test every dynamic handler used by completed slices.
- [ ] Implement talk, search, door, stair, inspect, and event-trigger interactions.
  - [ ] Define selection rules for facing tiles, nearby entities, and current location.
  - [ ] Establish priority when several interaction types could apply.
  - [ ] Emit the correct message or event without double-triggering movement transitions.
  - [ ] Test each interaction with valid, invalid, blocked, and story-modified targets.
- [ ] Implement NPC movement and interaction timing required by reachable maps.
  - [ ] Capture autonomous movement rules, delays, direction choices, and collision behavior.
  - [ ] Give NPCs deterministic RNG access where random movement is evidenced.
  - [ ] Define behavior while dialogue, menus, events, or transitions pause the field.
  - [ ] Test player/NPC contention for the same tile and interaction during movement.
- [ ] Build a native event/state model for flags and scripted sequences without interpreting 6502 code.
  - [ ] Define typed flags, counters, conditions, commands, and event identifiers by gameplay meaning.
  - [ ] Implement only native authored event operations required by observed behavior.
  - [ ] Make event progress deterministic, inspectable, serializable, and testable.
  - [ ] Reject a generic 6502 bytecode interpreter or direct assembly execution path.

### Field Menus and Progression

- [ ] Implement party status, items, equipment, spells, tactics, and formation menus.
  - [ ] Define data models and navigation trees for each menu.
  - [ ] Reuse window, list, cursor, help, and target-selection components.
  - [ ] Preserve enabled/disabled states, ordering, paging, cancellation, and return focus.
  - [ ] Test empty, one-entry, full, and overflow/page-boundary states.
- [ ] Implement item use, transfer, equip/unequip, discard, and inventory-capacity rules.
  - [ ] Derive eligibility, target, consumption, ownership, and capacity rules from evidence.
  - [ ] Make each operation transactional so cancellation or failure leaves state unchanged.
  - [ ] Handle cursed, unique, plot, equipped, full-inventory, and invalid-target cases.
  - [ ] Test operation messages and persistence after save/load.
- [ ] Implement spell eligibility, costs, targeting, effects, and field restrictions.
  - [ ] Define typed spell data and learned-spell ownership.
  - [ ] Validate caster state, MP, location, target, and story restrictions before cost is consumed.
  - [ ] Apply effects and RNG in evidenced order with exact clamping and failure messages.
  - [ ] Test every field spell and all ineligible paths.
- [ ] Implement shops, inns, churches, banks/vaults, and other standard location services.
  - [ ] Define each service's entry conditions, menus, pricing, state changes, and exit paths.
  - [ ] Use typed inventories, prices, party state, and save operations rather than UI-owned values.
  - [ ] Handle insufficient funds, full inventory, invalid equipment, dead/cursed members, and cancellation.
  - [ ] Add complete interaction tests for each service type.
- [ ] Implement gold, experience, levels, derived statistics, and learned-spell progression.
  - [ ] Reconstruct numeric widths, caps, growth tables, and calculation order.
  - [ ] Define when experience is awarded and which party/reserve members receive it.
  - [ ] Apply level-ups, stat gains, HP/MP changes, and learned spells in reference order.
  - [ ] Test exact thresholds, multiple levels at once, caps, and save/load continuation.
- [ ] Add deterministic tests for each menu action and persistence round trip.
  - [ ] Test arrange/action/result for each command without SDL.
  - [ ] Verify cancellation and failure paths produce no unintended mutation.
  - [ ] Serialize, reload, and compare state after every persistent operation category.
  - [ ] Include readable failure context naming menu, selection, and target.

### Encounters and World Travel

- [ ] Implement encounter zones, terrain modifiers, step accounting, RNG consumption, and encounter selection.
  - [ ] Map world/location cells to encounter zones and formation tables.
  - [ ] Reconstruct step counters, terrain rates, suppression states, and RNG call order.
  - [ ] Select only valid formations for current chapter, vehicle, and story state.
  - [ ] Test fixed seeds over known movement paths against reference observations.
- [ ] Implement world-map traversal and every vehicle/transport mode when first reached by a complete slice.
  - [ ] Define movement capabilities, speed, terrain permissions, boarding, landing, and disembarking per vehicle.
  - [ ] Keep vehicle state and party location persistent and mutually consistent.
  - [ ] Handle map transitions, blocked destinations, encounters, and follower/wagon behavior.
  - [ ] Test every terrain edge and save/load while using each vehicle.
- [ ] Implement day/night or progression-dependent world changes when first encountered.
  - [ ] Define deterministic world-time advancement and pause conditions.
  - [ ] Apply palette, NPC, door, service, encounter, and dialogue changes from time/story state.
  - [ ] Handle transitions across time thresholds while inside and outside locations.
  - [ ] Test save/load and travel at every relevant state boundary.
- [ ] End this phase with a replay that travels between multiple locations and reaches a battle transition.
  - [ ] Choose a route covering world movement, location entry, interaction, menus, and encounter triggering.
  - [ ] Record normalized inputs and state checkpoints throughout the route.
  - [ ] Compare position, flags, RNG, encounter selection, and final battle initialization.
  - [ ] Repeat in Debug and Release with identical results.

## Phase 5 - Battle Slice

Target: enter, resolve, and exit representative normal and boss battles with exact game-state outcomes.

### Battle Rules

- [ ] Define typed combatants, parties, formations, commands, actions, effects, resistances, and status conditions.
  - [ ] Separate immutable species/character definitions from mutable battle participants.
  - [ ] Define typed IDs and bounded numeric fields for every combat concept.
  - [ ] Model parties/formations, target sets, queued commands, resolved actions, and statuses explicitly.
  - [ ] Document ownership and lifetime from encounter initialization through cleanup.
- [ ] Implement encounter formation and battle initialization.
  - [ ] Resolve selected formation into enemy instances, positions, palettes, names, and initial state.
  - [ ] Copy relevant party state without losing field-only state needed after battle.
  - [ ] Seed battle RNG/context and apply preemptive/surprise/special encounter rules.
  - [ ] Test representative normal, grouped, arena, scripted, and boss formations.
- [ ] Implement command selection, targeting, tactics, turn ordering, agility, and RNG consumption.
  - [ ] Reproduce command menus and valid targets for each conscious participant.
  - [ ] Convert player choices and tactics decisions into immutable queued commands.
  - [ ] Reconstruct initiative calculations, ties, priority actions, and RNG call order.
  - [ ] Test mixed speeds, invalidated targets, incapacitation, and deterministic ties.
- [ ] Implement physical attacks, criticals, defense, damage formulas, misses, and special attack properties.
  - [ ] Translate each arithmetic step with correct integer width, truncation, clamp, and random range.
  - [ ] Determine hit, critical, defense, special weapon, and target-state ordering.
  - [ ] Apply damage and defeat only after the exact result is resolved.
  - [ ] Test minimum/maximum stats, zero damage, misses, criticals, lethal hits, and overflow boundaries.
- [ ] Implement spells, breath/actions, items, status changes, resistances, immunities, reflection, and absorption.
  - [ ] Define typed effect operations shared by spells, items, and monster actions where behavior truly matches.
  - [ ] Reconstruct target selection, resistance checks, effect magnitude, status duration, and message order.
  - [ ] Handle reflection, immunity, absorption, cure, stacking, replacement, and failure explicitly.
  - [ ] Add table-driven tests for every completed action against every resistance/result class.
- [ ] Implement monster AI patterns, weighted actions, summons, transformations, and phase changes.
  - [ ] Decode AI pattern IDs and weighted action tables into immutable definitions.
  - [ ] Reconstruct condition checks, weight rolls, invalid-action fallback, and RNG consumption.
  - [ ] Implement summon capacity, target formation changes, transformations, and boss phases as state transitions.
  - [ ] Test fixed-seed action distributions and every conditional branch.
- [ ] Implement victory, defeat, escape, experience, gold, drops, level-up, and post-battle state restoration.
  - [ ] Define terminal conditions and priority when several occur in one action.
  - [ ] Reconstruct escape eligibility/chance and consequences of success/failure.
  - [ ] Calculate rewards, drops, level-ups, and messages in evidenced order.
  - [ ] Restore field party state, statuses, music, position, events, and save eligibility correctly.
  - [ ] Test normal victory, wipe, escape, scripted outcomes, arena outcomes, and post-battle level gains.
- [ ] Cover chapter-specific rules and special-party behavior only when supported by cited evidence.
  - [ ] Inventory each exception and cite its controlling routine/data/trace.
  - [ ] Isolate exceptions behind named rules rather than unexplained conditionals.
  - [ ] Test the exception and the neighboring normal case.
  - [ ] Do not infer or invent chapter behavior merely to fill a gap.

### Battle Presentation

- [ ] Render battle backgrounds, monster compositions, palettes, presentation patches, and animations.
  - [ ] Load formation placement and monster image metadata into typed presentation records.
  - [ ] Reproduce backdrop, palette slots, origins, cropping, sprite overlays, and presentation patches.
  - [ ] Drive animations from deterministic battle/presentation state.
  - [ ] Compare representative formation screenshots and animation checkpoints.
- [ ] Render party/command/status windows, messages, damage values, targeting, and state changes.
  - [ ] Define layout for every window and state-dependent variation.
  - [ ] Show only information available to the original/player-selected ruleset.
  - [ ] Synchronize cursor, messages, numbers, and status indicators with resolved actions.
  - [ ] Test long names, maximum values, multiple statuses, and every target layout.
- [ ] Implement battle music transitions and action/victory/defeat sound effects.
  - [ ] Map semantic battle events to typed music and sound IDs.
  - [ ] Reproduce start, boss override, interruption, victory, defeat, and return-to-field transitions.
  - [ ] Preserve action sound ordering relative to animation and messages.
  - [ ] Compare emitted audio-event sequences against reference captures.
- [ ] Preserve presentation timing where it controls input windows, action order, or visible outcomes.
  - [ ] Identify timing that is merely cosmetic versus timing that gates rules/input.
  - [ ] Store rule-affecting timing in deterministic simulation frames.
  - [ ] Allow optional presentation acceleration only when outcomes/order remain unchanged.
  - [ ] Test normal and accelerated modes produce identical final battle state.

### Battle Verification

- [ ] Create authored fixtures for representative attacks, spells, statuses, AI patterns, drops, and escape attempts.
  - [ ] Invent names and data values while covering every arithmetic and branch category.
  - [ ] Set explicit initial RNG state and participant state.
  - [ ] Keep fixtures minimal so a failure identifies one behavior.
  - [ ] Validate fixture invariants before executing the tested rule.
- [ ] Compare native turn snapshots and RNG state against reference traces.
  - [ ] Capture pre-command, post-selection, action-order, post-action, and end-turn checkpoints.
  - [ ] Compare participant state, queued actions, statuses, rewards, and exact RNG state.
  - [ ] Report the first differing field and recent RNG/input history.
  - [ ] Resolve differences before adding more battle behavior.
- [ ] Add complete replays for one normal encounter, one multi-monster encounter, and one scripted boss.
  - [ ] Select encounters that collectively cover targeting, groups, statuses, rewards, and special flow.
  - [ ] Record commands for every turn and checkpoint each resolved action.
  - [ ] Continue each replay through return to field or scripted destination.
  - [ ] Verify repeated runs and save/load entry where applicable.
- [ ] Require zero unexplained state divergence before expanding battle content.
  - [ ] Classify each difference as native defect, extraction defect, evidence gap, or approved adaptation.
  - [ ] Add a regression test for every corrected divergence.
  - [ ] Document approved adaptations in `BEHAVIOR_NOTES.md`.
  - [ ] Block adjacent battle features while unexplained divergence remains.

## Phase 6 - Chapter and World Completion

Implement complete playable progression in story order so every newly exposed system is finished when introduced.

- [ ] Complete Chapter 1 progression, events, required battles, services, and chapter transition.
  - [ ] Define the chapter's initial state, required route, optional content, and terminal transition from reference evidence.
  - [ ] Complete every reachable map, interaction, menu, service, item, event, encounter, and battle before advancing.
  - [ ] Test alternate dialogue and event states caused by visiting locations or completing objectives in different valid orders.
  - [ ] Save/reload at representative checkpoints and compare a complete chapter-end snapshot against reference evidence.
- [ ] Complete Chapter 2 progression and newly introduced party/arena systems.
  - [ ] Inventory Chapter 2's party members, recruit/join state, tournament rules, and chapter-specific progression gates.
  - [ ] Implement each tournament opponent and transition through victory, defeat, and retry behavior.
  - [ ] Complete all Chapter 2 locations, events, services, optional discoveries, and required battles.
  - [ ] Verify chapter-end party, inventory, levels, flags, RNG, and transition state through replay checkpoints.
- [ ] Complete Chapter 3 progression and chapter-specific economy/drop behavior.
  - [ ] Reconstruct employment, shop ownership, sales, pricing, guards, contracts, and chapter-specific drop rules.
  - [ ] Track economy state explicitly and preserve it through save/load and time/progression changes.
  - [ ] Complete all Chapter 3 maps, dialogue branches, tunnels, services, events, and battles.
  - [ ] Test low/high money, full inventory, missed/optional transactions, and every required economic threshold.
- [ ] Complete Chapter 4 progression and newly introduced travel/event systems.
  - [ ] Inventory Chapter 4 party, companion, vehicle/travel, key-item, and performance/event mechanics.
  - [ ] Complete every reachable location, NPC/event branch, battle, and transition in chapter order.
  - [ ] Preserve temporary actor and story-state changes correctly through save/load.
  - [ ] Compare chapter-end state and transition into Chapter 5 against reference evidence.
- [ ] Complete Chapter 5 assembly of the final party, wagon behavior, tactics, vehicles, and world progression.
  - [ ] Implement recruit discovery, joining, wagon placement, active/reserve selection, and tactics ownership.
  - [ ] Implement every vehicle's acquisition, traversal rules, map representation, persistence, and restrictions.
  - [ ] Model world-state and story gates without hardcoded route skips.
  - [ ] Test valid party compositions and progression orders through every required Chapter 5 milestone.
- [ ] Implement all towns, castles, shrines, caves, towers, dungeons, world variants, and transition rules.
  - [ ] Create a location inventory from extracted maps and reference entry/transition tables.
  - [ ] Mark each map/submap complete only after rendering, collision, entities, interactions, events, encounters, and exits work.
  - [ ] Cover day/night, interior/roof, chapter/story, destruction/restoration, and other variants explicitly.
  - [ ] Add automated reachability/transition validation to detect broken destination IDs or coordinates.
- [ ] Implement all recruitments, party changes, temporary members, deaths, transformations, and event overrides.
  - [ ] Define typed party membership and actor-state transitions with evidence citations.
  - [ ] Preserve equipment, inventory, status, level, and wagon state according to each transition's rules.
  - [ ] Handle duplicate recruitment attempts, unavailable members, full active parties, and save/reload boundaries.
  - [ ] Test every transition in isolation and through its complete story sequence.
- [ ] Implement all bosses, scripted battles, special victory/defeat handling, and final battle phases.
  - [ ] Inventory boss formations, AI phases, scripts, music, presentation, rewards, and post-battle events.
  - [ ] Separate standard battle rules from explicitly evidenced boss exceptions.
  - [ ] Implement transitions between multi-phase forms without losing deterministic state or target validity.
  - [ ] Replay victory, defeat, escape restrictions, scripted interruptions, and post-battle progression for every boss.
- [ ] Implement Zenithia/endgame progression, ending sequence, staff credits, and return/termination behavior.
  - [ ] Complete endgame maps, traversal, key items, NPC states, events, and battle prerequisites.
  - [ ] Reproduce final battle entry, all phases, outcome state, and music/presentation transitions.
  - [ ] Implement ending scenes and credits from structured scene/text data rather than captured video or screenshots.
  - [ ] Define behavior after credits, including return to title, termination, save marking, or replay unlocks as evidenced.
- [ ] Add a versioned save migration whenever completed progression changes persisted data.
  - [ ] Increment format version only when serialized interpretation changes.
  - [ ] Define a one-way migration from every previously released version to the current version.
  - [ ] Preserve unknown/newer saves and backups instead of rewriting them.
  - [ ] Test each migration with pre-change fixtures and complete state comparisons.
- [ ] Maintain at least one deterministic replay/checkpoint chain per chapter.
  - [ ] Include chapter start, major milestones, required battles, chapter end, and transition checkpoints.
  - [ ] Record normalized inputs and explicit initial content/save versions.
  - [ ] Run every chapter replay in Debug and Release during acceptance.
  - [ ] Update or replace a replay only with documented evidence for the expected behavior change.

## Phase 7 - Optional and Specialized Systems

These are required for completion even when they are not on the shortest story path.

- [ ] Implement casino slot machines, poker, coins, rewards, and persistence.
  - [ ] Reconstruct wager limits, input flow, RNG order, odds, payouts, double-or-nothing, and exit behavior.
  - [ ] Implement casino coins as validated persistent state separate from ordinary gold.
  - [ ] Reproduce every visible screen, animation, message, sound, and insufficient-funds path.
  - [ ] Test deterministic outcomes with fixed RNG and save/load around casino activity.
- [ ] Implement tournament/arena flows and other chapter-specific battle presentations.
  - [ ] Inventory each arena's participant selection, wagering, AI-only/player-controlled rules, and termination conditions.
  - [ ] Implement presentation and messages that differ from standard battles.
  - [ ] Handle draws, overtime, disqualification, reward return, and story-specific outcomes.
  - [ ] Add full deterministic replays for each distinct arena mode.
- [ ] Implement Small Medal collection and reward progression.
  - [ ] Identify every medal source, one-time collection flag, count limit, and reward threshold.
  - [ ] Prevent duplicate collection and reward claims across save/load and world variants.
  - [ ] Implement reward choice/ordering and inventory-capacity behavior exactly.
  - [ ] Test zero, threshold-minus-one, exact threshold, all rewards, and completed states.
- [ ] Implement shops or services with specialized inventory, pricing, negotiation, or chapter rules.
  - [ ] Inventory every service that differs from the standard shop/inn/church/bank model.
  - [ ] Give each exception a named rule with direct evidence rather than a generic script byte path.
  - [ ] Implement all price, stock, availability, negotiation, and story-state branches.
  - [ ] Test edge money/inventory values and return visits after progression changes.
- [ ] Implement hidden items, optional maps, side events, optional recruits, and alternate dialogue states.
  - [ ] Build a completion inventory from map records, item/event flags, and dialogue branches.
  - [ ] Ensure optional content cannot block required progression when skipped or completed early.
  - [ ] Persist one-time discoveries, rewards, actors, and alternate states.
  - [ ] Test discovery before/after relevant story transitions and across save/load.
- [ ] Implement all tactics modes and AI-controlled party behavior.
  - [ ] Reconstruct each tactic's evaluation priorities, legal actions, target selection, and RNG behavior.
  - [ ] Share battle-rule validation so AI cannot choose actions a player could not legally execute.
  - [ ] Handle no-valid-action, low resources, statuses, reflected spells, and changing targets.
  - [ ] Compare fixed-state/fixed-seed decisions against reference traces for every tactic.
- [ ] Audit every extracted item, spell, monster, map, sprite set, music track, and sound effect for a runtime use or a
  documented reason it is not used.
  - [ ] Generate inventories from loaded content and record all typed IDs.
  - [ ] Record at least one owning runtime system or evidence-only classification for each entry.
  - [ ] Investigate unreferenced entries as unused, duplicate, placeholder, inaccessible, or missing implementation.
  - [ ] Fail the completion audit when an entry has neither a runtime use nor a reviewed disposition.

## Phase 8 - Save Compatibility and Resilience

- [ ] Finalize the native save schema after all persistent gameplay systems exist.
  - [ ] Inventory every persistent field used by title through ending and optional systems.
  - [ ] Remove transient caches, presentation state, pointers, and derivable values from the serialized contract.
  - [ ] Define canonical ordering, widths, bounds, defaults, optional sections, and integrity protection.
  - [ ] Freeze and document the first stable public save version only after full-game testing.
- [ ] Test all save slots, chapter boundaries, event flags, party variants, inventories, vehicles, casino state, and ending state.
  - [ ] Build a matrix covering every chapter start/end and major persistent subsystem.
  - [ ] Include empty/full slots, maximum values, unusual valid party states, and optional-content combinations.
  - [ ] Save in one process, load in another, and compare complete deterministic snapshots.
  - [ ] Run the matrix against every supported migrated save version.
- [ ] Add migration tests from every previously released native save version.
  - [ ] Retain authored, non-copyrighted fixtures representing each historical schema.
  - [ ] Migrate one version step at a time or define/test direct migration policy explicitly.
  - [ ] Compare every preserved field and documented default for newly introduced fields.
  - [ ] Test repeated loading does not reapply a migration destructively.
- [ ] Add backup recovery and interrupted-write tests.
  - [ ] Simulate failure before temporary write, during write, before replace, during replace, and after replace.
  - [ ] Verify at least one previous valid copy survives every simulated failure.
  - [ ] Detect and clean stale temporary files without deleting valid saves.
  - [ ] Exercise restore, decline, and erase choices through the user-facing recovery flow.
- [ ] Decide whether NES `.sav` import/export is a supported feature.
  - [ ] Define user value, legal/copyright boundary, compatible ROM/save variants, and maintenance cost.
  - [ ] Compare every NES save field with the final native schema and identify nonrepresentable values.
  - [ ] Decide import-only, import/export, unsupported, or separate-tool scope explicitly.
  - [ ] Record the decision and acceptance requirements before implementation.
- [ ] If supported, implement it as an offline conversion tool with exact field validation and no runtime ROM dependency.
  - [ ] Accept only recognized save sizes/layouts and reject ambiguous or corrupt input.
  - [ ] Convert through typed intermediate records rather than copying bytes into game memory.
  - [ ] Produce a separate output file and never overwrite input automatically.
  - [ ] Round-trip supported fields and report every dropped/adapted field.
- [ ] Keep all real and generated save files ignored and out of test fixtures.
  - [ ] Ignore known save extensions and all test-output/save-data directories.
  - [ ] Use authored fixtures containing invented values only.
  - [ ] Scan staged files and release archives for accidental saves.
  - [ ] Run the copyright boundary after save-tool testing.

## Phase 9 - Full-Game Parity and Completion Audit

- [ ] Build an inventory of every meaningfully named reference routine and map it to native behavior or a documented
  non-runtime concern; do not require one C++ function per routine.
  - [ ] Import the authoritative routine inventory and contract identifiers.
  - [ ] Assign each routine to a native behavior, content extraction concern, platform adaptation, evidence-only role, or unused disposition.
  - [ ] Allow many reference routines to map to one native domain operation and vice versa when justified.
  - [ ] Review every unmapped entry before declaring completion.
- [ ] Audit all routine contracts used by gameplay and cite them from native tests or traceability entries.
  - [ ] Identify every contract that informs a completed rule or state transition.
  - [ ] Link the native symbol and at least one verifying test/replay checkpoint.
  - [ ] Confirm implementation does not depend on undocumented register/RAM assumptions.
  - [ ] Revisit contracts when parity evidence contradicts static interpretation.
- [ ] Run full chapter replays and compare checkpoints for progression, party, inventory, flags, RNG, and save state.
  - [ ] Start each replay from a documented clean initial state or prior chapter endpoint.
  - [ ] Compare every stable state domain at milestones and chapter transitions.
  - [ ] Verify RNG continuity across field, menu, battle, event, and save/load boundaries.
  - [ ] Archive only authored replay inputs/expectations; keep copyrighted captures ignored.
- [ ] Run focused parity suites for field movement, menus, economy, encounters, combat, AI, events, and ending behavior.
  - [ ] Define a suite owner and evidence source for each subsystem.
  - [ ] Cover representative normal, boundary, failure, and chapter-specific cases.
  - [ ] Run focused suites before full replays so failures are localized.
  - [ ] Require every corrected parity defect to gain a regression case.
- [ ] Compare representative screens, animation timings, music loops, and sound-effect sequences.
  - [ ] Select checkpoints covering every renderer/audio feature and major scene type.
  - [ ] Compare logical pixels, draw ordering, frame durations, loop sample boundaries, and event ordering.
  - [ ] Generate actionable diffs rather than subjective visual/audio judgment alone.
  - [ ] Document approved platform adaptations and tolerances.
- [ ] Resolve every unexplained divergence or document an intentional native-platform deviation in `BEHAVIOR_NOTES.md`.
  - [ ] Reproduce and minimize each divergence before classifying it.
  - [ ] Determine whether evidence, extraction, implementation, or comparison tooling is wrong.
  - [ ] Fix unintended differences and add regression coverage.
  - [ ] State rationale and player-visible impact for every intentional difference.
- [ ] Test clean startup and actionable failure modes with missing, partial, stale, and corrupt extracted assets.
  - [ ] Exercise missing root, missing family, missing payload, malformed JSON, schema mismatch, invalid image/audio, and cross-reference failures.
  - [ ] Confirm no SDL scene begins when required content validation fails.
  - [ ] Verify each diagnostic identifies cause and repair command.
  - [ ] Confirm failed startup leaves no partial save/config mutation.
- [ ] Test extended play, repeated save/load, device reconnects, focus changes, and scene transitions for leaks or drift.
  - [ ] Run long deterministic/autoplay sessions through representative scenes.
  - [ ] Monitor memory, handles, audio voices, textures, and temporary files for unbounded growth.
  - [ ] Repeat device/focus/window transitions while gameplay and menus are active.
  - [ ] Compare state/timing before and after stress events for drift.
- [ ] Confirm no native binary imports or dynamically loads a reference tool, ROM, emulator, or assembly payload.
  - [ ] Inspect project dependencies and final binary imports.
  - [ ] Search runtime paths/configuration for reference, ROM, assembler, emulator, and 6502 execution dependencies.
  - [ ] Run the boundary check against release configuration and staged source.
  - [ ] Test the native executable from a package that contains no `reference/` directory.
- [ ] Confirm Git history and release staging contain no ROM, extracted asset, capture, or save payload.
  - [ ] Scan tracked history and current index for prohibited extensions and known asset paths.
  - [ ] Inspect submodules, archives, package manifests, and documentation attachments.
  - [ ] Run copyright checks before tags and after package creation.
  - [ ] Remove leaked payloads from history before publishing rather than only deleting the latest copy.

## Phase 10 - Packaging and Release

- [ ] Define the supported Windows versions, architectures, Visual C++ runtime strategy, and GPU/audio requirements.
  - [ ] Select minimum tested Windows version and x64/other architecture scope.
  - [ ] Decide dynamic versus static Visual C++ runtime deployment consistent with dependencies/licenses.
  - [ ] Define minimum graphics API/driver, display, controller, and audio-device requirements.
  - [ ] Test and document unsupported configurations and expected errors.
- [ ] Produce a release build containing only native binaries, required third-party runtime libraries, licenses, and docs.
  - [ ] Build from a clean tagged commit with Release configuration and pinned dependencies.
  - [ ] Copy only executable, required DLLs, third-party notices/licenses, and selected native documentation.
  - [ ] Exclude PDBs or publish them separately according to release policy.
  - [ ] Generate and record package hashes and a complete file manifest.
- [ ] Do not include a ROM, extracted assets, saves, traces, screenshots, audio, or reference build products.
  - [ ] Build packages in a clean staging directory outside generated asset and reference build trees.
  - [ ] Scan package names, extensions, nested archives, and file signatures.
  - [ ] Confirm documentation does not embed prohibited payloads beyond approved authored screenshots.
  - [ ] Fail packaging automatically when a prohibited path or extension appears.
- [ ] Provide a first-run asset setup flow that asks the user to build/extract from their own lawful ROM source.
  - [ ] Explain required reference version/hash without distributing or linking to unauthorized ROM content.
  - [ ] Let the user select or provide their local source through a clear setup command/UI.
  - [ ] Verify source, run extraction locally, and store output in a user-owned ignored/untracked location.
  - [ ] Report progress, required dependencies, recoverable errors, and completion.
- [ ] Validate asset-set compatibility without retaining or uploading user ROM data.
  - [ ] Store nonreversible schema/tool/content-manifest version identifiers rather than ROM payloads.
  - [ ] Validate required catalogs, payload hashes where appropriate, and cross-references locally.
  - [ ] Avoid telemetry/crash logs containing paths, decoded text, images, audio, or ROM bytes.
  - [ ] Explain how users can re-extract incompatible sets.
- [ ] Add version information, crash/error logging, and a diagnostics report that excludes copyrighted payloads.
  - [ ] Embed application version, commit/build identifier, configuration, and content-schema version.
  - [ ] Log subsystem errors and stack/context information without game text/assets/save contents.
  - [ ] Define log rotation, storage path, opt-in sharing, and user redaction guidance.
  - [ ] Test diagnostics generated from startup, rendering, audio, input, content, and save failures.
- [ ] Run the full build, test, copyright, replay, save-migration, and packaging gates from a clean checkout.
  - [ ] Clone with submodules into a fresh path without relying on existing caches beyond documented dependency tools.
  - [ ] Execute reference build/verification, native Debug/Release builds, unit/integration tests, and boundary checks.
  - [ ] Run all chapter/focused replays and save migration/recovery matrices.
  - [ ] Create the package, scan it, install/run it, and repeat critical smoke tests.
- [ ] Document installation, asset preparation, controls, save location, troubleshooting, and uninstall behavior.
  - [ ] Write steps for a user unfamiliar with Visual Studio or the repository layout where appropriate.
  - [ ] Document default controls, rebinding, display/audio settings, and accessibility options.
  - [ ] Document asset/config/save/log locations and what uninstall does or leaves behind.
  - [ ] Include common errors with exact corrective actions and no copyrighted downloads.
- [ ] Tag a release only when the complete game is playable from title screen through ending with no placeholder path.
  - [ ] Complete every required phase and acceptance gate in this roadmap.
  - [ ] Perform at least one clean full-game playthrough and deterministic chapter replay suite.
  - [ ] Resolve release-blocking bugs, unexplained divergences, placeholders, and inactive controls.
  - [ ] Review staged tag contents, release notes, package hashes, and copyright boundaries before tagging.

This roadmap is complete only when every required gameplay path and release gate above is finished and validated.
