# Dragon Warrior IV Native

Dragon Warrior IV Native is a C++ reimplementation of the NES game, guided by the completed disassembly under
[`reference/`](reference/README.md). The native executable does not link, load, interpret, or call 6502 assembly.
Assembly and emulator-assisted tooling are evidence used during development only.

This repository does not contain ROM images or extracted copyrighted assets. A local exact ROM rebuild feeds the
tracked extractor, which writes ignored PNG, WAV, VGM, JSON, and TSV files for the native runtime.

## Prerequisites

- Windows and Visual Studio 2026 with the Desktop development with C++ workload
- Git with submodule support
- Python 3 with `numpy` for asset extraction

## Reference Status

- Reassemblable assembly: 100% (verified: build reproduces SHA-256 `373BE958CB33651FE599A6B282D2A232EB3B99559C258B2C70B53DF0FA31E34A`)
- Detailed semantic classification: 100% (524,288 / 524,288) - Done
- Meaningfully named routines: 4,562/4,562 (100%)
- Semantic contracts: 4,562/4,562 (100%)
- Pointer recovery, indirect-jump audit, and analyzer-warning disposition: 100%
- Current analyzer warnings and control-flow conflicts: 0

These three figures describe the completed classification partition rather than separate progress targets:
163,493 instruction bytes + 361,313 content-range bytes - 518 bytes in both inventories = 524,288 classified
PRG bytes.

## Semantic Status

All 4,562 verified interfaces have a curated name and a contract synchronized with the generated interface
inventory. The semantic notes behind the derived contracts have not been re-reviewed for the 1,331 interfaces
that the 2026-10-01 body-traversal corrections widened. The authoritative
per-bank completion table, audit history, evidence checks, and remaining work are maintained in
[reference/docs/STATUS.md](reference/docs/STATUS.md); detailed audit findings remain in
`reference/analysis/audits/`.

## Quick Start

Initialize the moved reference submodule after cloning:

```bat
git submodule sync --recursive
git submodule update --init --recursive
python -m pip install -r scripts\port\requirements-assets.txt
```

Visual Studio's bundled vcpkg restores SDL3, SDL3_image, nlohmann-json, and Catch2 from the pinned manifest.

## Build And Test

```bat
build-native.cmd
test-native.cmd
```

`build-native.cmd` defaults to Release. Select a configuration explicitly with `build-native.cmd Debug` or
`build-native.cmd Release`; each builds the same native solution and its complete project references.

In Visual Studio, open [DragonWarrior4.Native.sln](DragonWarrior4.Native.sln), select x64 and Debug or Release,
and set `DW4.Desktop` as the startup project. Tracked desktop settings launch `$(TargetPath)` from the repository
working directory for both configurations. Reload the solution after external project-file changes so new source
items and debugger defaults are visible. Native outputs are `build\Debug\DW4.Desktop.exe` and
`build\Release\DW4.Desktop.exe`; the separate reference solution does not build the native executable.

The native solution is [`DragonWarrior4.Native.sln`](DragonWarrior4.Native.sln). Outputs are written under
`build\Release`. The reference solution remains separate under [`reference/`](reference/DragonWarrior4.sln).

The test command runs native rule/content tests and the authored catalog-preparation regression suite.
It also verifies the headless replay command with matching and deliberately divergent authored checkpoints.
Debug tests link the Debug Catch2 libraries independently of the Release configuration.

## Extract Assets

```bat
extract-native-assets.cmd
```

The command rebuilds and exact-verifies the reference ROM, then exports assets to the ignored
`native\assets\generated` directory. Individual extractor stages are also supported:

```bat
extract-native-assets.cmd --stage text --stage maps
```

After a complete extraction, launch from the repository root:

```bat
build\Release\DW4.Desktop.exe
```

Launching directly from `build\Debug` or `build\Release` is also supported: default development assets are discovered
relative to the executable. Installed builds can use an adjacent `assets` folder; `--assets` always overrides discovery.

Validate the current content contract without opening an SDL window:

```bat
build\Release\DW4.Desktop.exe --validate-assets
build\Release\DW4.Desktop.exe --assets "C:\My Local Assets" --validate-assets
```

This now also decodes and validates every graphics/font PNG. Inspect authentic glyphs and recolored/flipped tiles
rendered natively into an ignored logical-size image:

```bat
build\Release\DW4.Desktop.exe --render-check build\native-render-check.png
```

The inspection image is not a title-screen implementation or a gameplay background. Keep all rendered inspection
images under ignored build/capture storage. Pixel tests run through SDL's software renderer without a GPU.

Run a local foundation replay without opening an SDL window:

```bat
build\Release\DW4.Desktop.exe --replay build\my-replay.json
```

Replays restore complete standard-engine RNG state, normalized inputs, and sparse checkpoints. A mismatched checkpoint
reports the first frame and field and exits with code 2. This currently validates the deterministic foundation, not
gameplay or reference parity. Native random choices use the approved standard-library adaptation documented in
[behavior notes](docs/port/BEHAVIOR_NOTES.md).

Extraction now prepares schema-version-1 catalogs. Existing unversioned asset sets must be re-extracted or passed
through the developer-only preparation command:

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\port\prepare-native-asset-catalogs.ps1 -OutputPath native\assets\generated
```

The desktop provides a native title/adventure-log flow and the Chapter 1 opening route through Burland castle and town
to the world map. Gameplay uses typed map, actor, item, dialogue, encounter and progression data with native rule
services; it does not execute the reference ROM or emulator. The remainder of the complete port is tracked in
[`PORTING_STATUS.md`](docs/port/PORTING_STATUS.md).

The normal desktop launch now shows the native opening/title presentation with its verified music cue and animated
scrolling. Structured title patterns, palettes and draw records are prepared by `extract-native-assets.cmd`; existing
asset sets can prepare only title data from an already rebuilt local ROM:

```bat
python scripts\port\extract-native-title.py --rom "reference\build\Release\Dragon Warrior IV (USA).nes" --out native\assets\generated\title
```

Inspect or verify a title checkpoint with:

```bat
build\Release\DW4.Desktop.exe --title-check build\native-title.png --title-frame 1556
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\port\test-native-title.ps1 -Configuration Release
build\Release\DW4.Desktop.exe --run-frames 600
```

Title dismissal enters the adventure-log menus. Begin creates a version-2 gameplay save and enters the original Chapter 1
opening messages. Continue restores Chapter 1 gameplay state. Castle/town actors, their dialogue/effect programs,
merchant purchases/sales, inn/healing services, treasure searches, world encounters and physical battle rewards are
implemented for this route. Field A opens commands; TALK interacts with the actor ahead, including across counters.
STATUS displays HP/MP, all primary attributes, derived attack/defense, EXP and the next threshold. ITEM supports
equipment, supported consumables and discard. SAVE and return-to-title use the native slot store.

Keyboard defaults remain Z = A/confirm, X = B/cancel/delete, Enter = Start, Right Shift = Select, arrows = directions,
and Escape = quit. SDL gamepads are opened and polled: DualSense Cross = A, Circle = B, Options = Start, Create = Select;
D-pad or left stick navigate. Devices can connect/disconnect during play, and focus regain requires neutral input before
held buttons can act. Keyboard and gamepad samples become the same deterministic eight-button mask.

Native adventures are stored under SDL's per-user application-data directory for `NativePort/DragonWarrior4`,
in its `saves` subfolder; startup prints the exact local path. `--save-dir <directory>` is available for isolated tests.
Versioned JSON uses zlib CRC integrity and full standard-engine RNG state. Writes are validated, flushed, atomically
replaced, and retain a previous backup. Version 2 carries roster/statistics, party, inventory, story flags, position,
actors and opening-event state. A legacy version-1 profile initializes a fresh Chapter 1 game; it never contained
gameplay progress. Quitting during battle retains its pre-battle checkpoint, not an incomplete combat snapshot.
Backup recovery UI, other chapters, wider story progression and full battle/monster-AI parity remain unfinished.

Prepare only the opening-route data from an already rebuilt local ROM and existing extracted maps/text:

```bat
python scripts\port\extract-native-opening.py --verify-curves
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\port\test-native-gameplay.ps1 -Configuration Release
build\Release\DW4.Desktop.exe --field-check build\native-field.png
```

The gameplay gate drives real input/session code through the opening, checks every loaded actor dialogue/choice
program, navigates castle/town/world, verifies a full-state reload, and earns battle rewards and level 2 through play.
World/battle captures are checked for nonblank 256x240 output. Generated content, saves and captures remain ignored.

## Copyright Boundary

Only authored source, extraction tooling, schemas, and behavioral evidence belong in Git. ROM images and extracted
PNG, WAV, VGM, JSON, TSV, text, or packaged runtime payloads must remain ignored. `extract-native-assets.cmd` refuses
an in-repository output directory unless Git ignores it, and `build-native.cmd` rejects exposed or force-added payloads.
Do not bypass these checks with `git add -f`.

## Layout

| Path | Purpose |
| --- | --- |
| `reference/` | Unmodified disassembly source, evidence, ROM build, and extraction tools |
| `native/game/` | Deterministic gameplay library with no platform dependencies |
| `native/content/` | Typed readers and validation for generated assets |
| `native/desktop/` | SDL3 Windows executable and platform adapters |
| `native/assets/` | Tracked contracts plus ignored generated asset payloads |
| `tests/` | Native unit, integration, replay, and future parity tests |
| `docs/port/` | Native architecture, evidence rules, and implementation status |
| `scripts/port/` | Native build-policy and support tooling |

## Documentation

### Native Port

- [Architecture and dependency boundaries](docs/port/ARCHITECTURE.md)
- [Current implementation status](docs/port/PORTING_STATUS.md)
- [Behavior reconstruction rules](docs/port/BEHAVIOR_NOTES.md)
- [Routine-to-native traceability](docs/port/routine-map.tsv)
- [Generated asset contract](native/assets/schemas/README.md)

### Development Notes

- [Work to be done](docs/DevNotes/WorkToBeDone.md)
- [Development diary](docs/DevNotes/DevDiaryNotes.md)
- [AI use tracking](docs/DevNotes/AI_UseTracking.md)

### Code Study Notes

- [Code notes index](docs/DevNotes/CodeNotes/CodeNotes.md)
- [Content: `AssetCatalog.hpp` and `AssetCatalog.cpp`](docs/DevNotes/CodeNotes/Content/AssetCatalog.md)
- [Desktop: `main.cpp`](docs/DevNotes/CodeNotes/Desktop/main.md)
- [Game: `Game.hpp` and `Game.cpp`](docs/DevNotes/CodeNotes/Game/Game.md)
- [Tests: `GameTests.cpp`](docs/DevNotes/CodeNotes/Tests/GameTests.md)
- [Tests: `AssetCatalogTests.cpp`](docs/DevNotes/CodeNotes/Tests/AssetCatalogTests.md)
