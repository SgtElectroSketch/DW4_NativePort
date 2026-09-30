# Native Port Status

## Implemented

- Existing disassembly, analysis, scripts, and tools moved intact under `reference/`.
- Reference solution builds from its new location and reproduces the exact expected SHA-256.
- Visual Studio native solution with `DW4.Game`, `DW4.Content`, `DW4.Desktop`, and `DW4.Tests`.
- C++23, x64 Debug/Release, strict warnings, pinned vcpkg dependencies, and SDL3 deployment.
- Build-time rejection of assembly inputs and references outside native/test ownership.
- Deterministic frame/input foundation at the NES NTSC cadence.
- Startup validation of required top-level text, map, sprite, monster, graphics, screen, and audio JSON catalogs.
- Local asset pipeline from exact ROM rebuild to ignored PNG/WAV/VGM/JSON/TSV files.
- A unit test for frame advancement and integration tests for the asset catalog contract.

## Not Yet Implemented

- Title sequence, adventure-log menus, name entry, or chapter startup.
- Map rendering, collision, field entities, dialogue, events, and random encounters.
- Battle simulation and presentation.
- Music/SFX playback, save persistence, controller mapping, or configuration UI.
- Reference replay capture and behavior-parity gates.

The desktop executable is an infrastructure host at this stage, not a playable build.

## First Gameplay Slice

The first implementation slice should be complete from title screen through new-game creation into the opening playable
scene, including name entry, save creation, input, rendering, dialogue, and the relevant event state. Each behavior must
cite reference evidence and gain a deterministic test before the next gameplay slice begins.
