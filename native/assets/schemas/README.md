# Generated Asset Contract

The current `DW4.Content` startup validation expects these catalogs from a complete run of
`extract-native-assets.cmd`:

- `text/dialogue.json`
- `maps/index.json`
- `sprites/field/index.json`
- `monsters/index.json`
- `graphics/index.json`
- `screens/index.json`
- `audio/index.json`

The extractor documents detailed records inside each JSON file. Native loaders must validate those records and convert
them to typed immutable data before gameplay uses them. Generated payloads belong in `../generated/` and are ignored;
do not commit PNG, WAV, VGM, decoded text, map data, or other extracted game content. Runtime and package copies must
remain under ignored build/package paths and are subject to the repository copyright-boundary check.

## Version 1

Every consumed root catalog requires the integer field `schema_version` with value `1`. The developer-only preparation
step adds this marker after extraction; missing, unsupported, or incorrectly typed versions fail native startup with
re-extraction instructions. Optional provenance and preview fields remain permitted. Changes to required field
interpretation require a new version and matching producer/consumer updates; unknown versions are never downgraded.

The authored [schema resource](catalogs-v1.schema.json) contains one definition per catalog:

| Catalog | Definition | Current Native Validation |
| --- | --- | --- |
| text/dialogue.json | `#/$defs/dialogue` | Required raw records, integer ranges, unique IDs, group/index relationship, byte symbols |
| maps/index.json | `#/$defs/maps` | Locations array containing object records |
| sprites/field/index.json | `#/$defs/sprites` | Sets array containing object records |
| monsters/index.json | `#/$defs/monsters` | Monsters array containing object records |
| graphics/index.json | `#/$defs/graphics` | Typed PNG descriptors, 1bpp/2bpp formats, tile counts, glyph IDs/labels, path containment and uniqueness |
| screens/index.json | `#/$defs/screens` | Reference-scenes array containing object records |
| audio/index.json | `#/$defs/audio` | Music and SFX arrays containing object records |

Native checks additionally require integer-valued fields to use integer JSON tokens, rejecting floating-point
encodings even when mathematically integral. Dialogue IDs must equal `group * 32 + index`. Raw symbol streams are
retained without interpreting control commands or rendering preview strings. Lookup returns immutable typed records,
not JSON, ROM addresses, or extractor bit offsets.

The non-dialogue schemas intentionally describe only fields currently consumed by startup validation. Complete domain
records, asset dimensions, palette bounds, referenced files, and cross-catalog identifiers are still required work.
Captured screens are comparison evidence, not native gameplay backgrounds; map PNG previews likewise must not replace
cell/metatile rendering. WAV playback is a native platform responsibility; VGM and original sound-engine execution
remain developer-only evidence.

Graphics images are decoded by SDL3_image with its explicit `png` feature. CHR basenames resolve beneath
`graphics/chr`; font paths resolve beneath `graphics`. Source paths, ROM addresses, and loader labels are not runtime
graphic identities. Both lexical parent traversal and canonical path escape are rejected.

Sheets have sixteen 8x8 tiles per row. Four-color extractor sheets use exact gray levels 0/85/170/255; monochrome
sheets use 0/255. Native loading recovers palette indexes and validates opaque source pixels, tile-sheet height,
bounded image dimensions, and font tile capacity. Runtime palettes may assign transparency and color without
mutating the original data. Actual SDL palette conversion, textures, nearest scaling, flips, and clipping provide rendering.

## Foundation Replay Version 1

The authored [replay schema](replay-v1.schema.json) covers the implemented `foundation-1` replay format. It carries
an initial frame/input/edge snapshot, complete `std::mt19937` engine state, draw count, one normalized input record per
frame, and sparse expected snapshots. Engine text is written/read by the standard stream operators with the classic
locale, not by an authored binary codec. nlohmann-json owns document parsing and serialization.

The compatibility header includes format version, native rule version, asset schema version, and compiler/standard-library
identity. Unsupported compatibility is rejected before execution. Native checks additionally reject fractional integer
tokens, malformed or trailing engine data, inconsistent button edges, counter overflow, duplicate/out-of-order checkpoint
indexes, more than 360,000 input frames, or more than 1,000,000 aggregate explicit RNG draws. Replay files are capped at
64 MiB. Checkpoints are evaluated after their zero-based input-frame index is applied.

`random_draws` exercises the foundation RNG service explicitly; it does not encode or claim original gameplay call timing.
The full party/inventory/location/event/battle snapshot, content-set identity, reference capture comparison, readable value
diffs, and version migration remain required work. User replay outputs belong in ignored build/capture storage.

## Title Version 1

The authored [title schema](title-v1.schema.json) describes `title/index.json` and its indexed `patterns.png`.
The developer-only extractor runs the verified reference path under strict frame/process bounds and normalizes
observations into tile IDs, RGBA palettes, signed positions, flips and clip bands. Shared draw records reduce repeated
JSON data. The runtime does not read screenshot pixels, PPU registers, NES RAM, CPU instructions, or emulator state.

Observed boundaries include artwork input readiness at frame 364, a 25-frame exit interval, and track 2 at frame 349.
The idle tail repeats every 1,024 frames after completed artwork. Comparison PNGs are evidence-only and remain ignored;
the optional local parity command compares all 61,440 pixels at five fixed checkpoints.

The current desktop presents opening/artwork and native WAV music and connects title dismissal to adventure logs.
Full opening gameplay, interruption/resume audio policy, and complete gameplay saves remain open.

## Adventure Profile Version 1

The authored [adventure profile schema](adventure-save-v1.schema.json) covers the current eight-glyph hero name,
gender, message speed, chapter identifier, logical frame and native RNG state. It is not yet a complete game save.
zlib CRC-32 checks the canonical serialized payload, and unknown versions or incompatible RNG formats are rejected.
Desktop owns the three-slot store, per-user paths, Windows atomic replacement/flush, backups, copy and erase operations.
Raw object memory and NES SRAM addresses are never serialized into the native container.

Party/character statistics, inventory, flags, location, vehicles, battle state, migrations and backup recovery UI remain
required extensions. Continue is not exposed until the opening scene and its complete persisted state are implemented.

## Gameplay Save Version 2

The [gameplay save schema](adventure-save-v2.schema.json) extends the profile envelope with all nine character records,
party IDs, HP/MP and primary attributes, experience, equipped inventories, bag, currency, story flags, map/submap/tile
position, facing, actor movement/script state and opening-event state. Character identity order, unique nonempty party,
HP/MP relationships, actor uniqueness and exact RNG compatibility are additionally enforced in native code.

Chapter 1 Continue now consumes this state. Version-1 profiles remain readable and initialize a fresh Chapter 1 game;
they never contained field progress. Autosaves and explicit field saves use the existing flushed atomic slot replacement.
Active combat retains a pre-battle checkpoint rather than claiming that partial field state is a full combat snapshot.
Other chapters, complete combat snapshots, vehicles and backup recovery UI remain explicit unfinished work.

## Opening Field Version 1

The developer exporter creates `opening/index.json` and an indexed neutral `patterns.png` from the existing map/sprite/
monster extraction helpers and reviewed consumers. Runtime `FieldData` validates bounded grids, palettes, tile references,
actor identity/positions/patrols, effect-tree depth, map destinations, item metadata, merchant stock, inn prices, treasure
records, all 99 levels of each progression curve, encounter tables and monster rules/presentations.

The current data set contains four Burland submaps and the main world grid. Each actor has a normalized interaction
program; reviewed callbacks use semantic IDs, not executable addresses. Map PNG previews and reference captures remain
evidence-only. Runtime rendering assembles metatiles and sprites. The optional curve gate compares 16 original routine
outputs with normalized data in developer tooling only; no original instructions or emulation run in the native game.
