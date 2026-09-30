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

The native solution is [`DragonWarrior4.Native.sln`](DragonWarrior4.Native.sln). Outputs are written under
`build\Release`. The reference solution remains separate under [`reference/`](reference/DragonWarrior4.sln).

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

The desktop program currently provides the native runtime, fixed-frame clock, input, SDL3 presentation surface, and
typed top-level asset-catalog validation. Gameplay behavior has not yet been implemented; see
[`PORTING_STATUS.md`](docs/port/PORTING_STATUS.md).

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
