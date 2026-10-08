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

## Approved RNG Adaptation - 2026-10-08

The project owner authorized modern implementations rather than reproducing primitive ROM infrastructure.
Native random choices use `std::mt19937` and `std::uniform_int_distribution`, not a custom recreation of the
fixed-bank feedback routine. Engine state and a 64-bit count of requested draws belong to deterministic snapshots;
restoring a snapshot continues the same native sequence. Clocks and platform entropy are not read inside the engine.

The original `$1F:$C891-$C8CB` uses two CRC-style byte updates and an 8-bit counter; `$1F:$D8ED-$D8F2` also
demonstrates a consumer reading the low state byte directly. These are evidence for original behavior, not native
architecture requirements after this approved adaptation. Individual native rolls and resulting playthroughs are
not expected to match the ROM's exact random stream. Rule parity must supply the same explicit random choices;
native replay determinism remains required. Original tables, probability rules, targeting, and arithmetic are not
changed by this approval.

Standard-library distribution mappings can differ between implementations. Persisted replays must identify the
native rules/toolchain compatibility version and carry complete engine state; a seed alone is not a continuation
snapshot. Title/new-game seed policy and per-frame scheduling still require a defined native contract.

## Native Menu/Input Adaptation - 2026-10-08

The owner authorized keyboard/gamepad input integration. Current keyboard defaults are preserved and SDL gamepads
use positional South/East for A/B, Start/Back for Start/Select, and D-pad/left-stick directions. Menu confirmation and
cancel act on press edges; opposite directions cancel. Native directional repeat is currently configurable with an
18-frame initial delay and six-frame interval; exact reference repeat/input timing is not claimed yet.

The adventure-log view uses extracted glyphs and opaque, ordered windows instead of a replacement full-screen panel.
Command order follows the dispatch table at `$12:AACA-AAD3`: Continue, Change Message Speed, Begin New Quest, Copy,
Erase, filtered by available operations. Continue is hidden until opening gameplay and complete persisted state exist.
Current saves are versioned native profiles, not full-game or NES SRAM saves. Native error text and additional profile
confirmations remain adaptations; pixel parity is not claimed for every possible menu state.

## Adventure Window And Speed Correction - 2026-10-08

Window record pointers at `$16:A5A7-A5B6` resolve records `$53-$5A` to `$AA07-$AA45`. The frame/cursor consumers at
`$16:A334-A355` and `$16:A446-A476` establish 16-pixel header coordinate units and twice the first byte's high nibble
as the tile width. Native command, empty-slot, occupied-slot, keyboard, name, speed, and gender windows now use those
placements and widths. Parents are emitted before opaque children; cancel/No restores the preceding page and cursor.
Name-entry B still deletes a character. The special name frame follows `$16:A3C8-A3F9`, including underline glyph
`$65` and cursor glyph `$85`, rather than an ordinary bottom border.

The existing adventure-log gate compares command, empty-slot, and empty name-entry screenshots with the locally
extracted reference captures at frames 2079, 2176, and 2283. Each is required to match all 61,440 pixels when those
reference images are available; missing images skip only the local pixel comparison, not functional/native tests.

The battle-message consumer at `$12:85CA-8636` reads stored speed `$62EE`: indices 0-6 select the seven delays at
`$12:8614-861A` (15, 21, 27, 33, 40, 46, 54 frames). Index 7 bypasses timed waiting and polls confirmation instead.
The menu retains the extracted one-row choices and Fast/Slow wording, with an added explicit manual label: displayed
1 is fastest, displayed 7 is slowest automatic, and displayed 8 is manual advance. This verifies the setting's direction
and battle-message semantics; a complete native dialogue/battle consumer is still future work.

## Chapter 1 Opening Route - 2026-10-08

The owner expanded the slice to reach the world map with actors/dialogue/interactions, money, leveling and all primary
statistics. Native services now separate dialogue, field navigation, effect execution, inventory/equipment, progression,
encounters and physical battle rules. Desktop owns content adaptation, views/devices and atomic save calls. No production
code reads original instructions, ROM addresses, emulator state or comparison screenshot pixels.

Reviewed initialization evidence: `$1E:A578-A598` consumes the chapter start table at `$A599`, selecting map 2/submap 1,
tile `(16,21)`. `$12:8EAD-91DC` initializes chapter party/character state, including Ragnar (ID 6), level 1, 27 HP,
strength 7, agility 4, vitality 13, intelligence 1, luck 2, 50 gold and equipped item IDs 2/38. The typed rule model retains
all nine roster identities and saved attributes; characters outside the active chapter are initialized for their chapter
when those later chapters are ported, not falsely presented as playable now.

Dialogue controls are grounded in `$16:8094-81C6`: word-aware glyph layout, explicit advance prompts and character/
lookup substitutions are shared services. The reference observation records original messages `$0300/$0301/$0302` for
the opening. Every loaded actor's dialogue branches and Yes/No effect paths have a bounded native execution/layout gate.
The effect interpreter's zero-success convention (`$15:99CE-9A7A`) is preserved; flag predicate tests distinguish before/
after quest state. Reviewed callbacks are normalized to semantic actions, never native jumps to original addresses.

Map actor coordinates come from `$1C:99E2-99F8` (record bytes 5/6), not record bytes 1/2. Behavior properties are cached
by `$08:8452-8467`; movement blocking is consumed at `$1D:9916-9925`. The reference walk-out trace enters town at
`(19,10)` from the castle; world routing is `(164,46)`. Native rendering assembles actual exported metatiles, roof groups,
directional sprites and monster tile records, using SDL palette/texture APIs. Field timing/presentation is not claimed
pixel-exact for the entire opening or all original animation/event scheduling.

Progression descriptors are consumed by `$12:9F7C-A0FA`; 16 developer-only comparisons verify EXP/attribute outputs at
levels 2/3/10/99. Ragnar's early EXP thresholds are 12/36/84/156. `$10:A20A-A23C` samples the original 32-nibble growth
factor; native code uses standard RNG sampling with that distribution and the `/121` gain scaling. Derived attack/
defense follow `$10:85BB-8620`; starting Copper Sword/Leather Armor give attack 19 and defense 14. Slot/class and price
tables are reviewed through `$10:8AE7-8C17`; sells use three quarters of base price (`$15:B35F-B47C`). Burland inn costs
4 gold per party member (`$18:B5C2`); poison treatment costs 5 (`$15:B5C7-B5D1`).

World encounter selection is grounded in `$18:9BE6-9F34`, with region/terrain/formation data exported to typed records.
Physical damage uses `$11:99C0-9A83`; turn initiative uses `$11:BEB2-BEE9`. The native physical loop supports target
selection, parry/run, defeat, authentic monster art, reward/drop application and real level gains. It remains a bounded
opening-route battle implementation, not a claim that every original monster AI, spell, critical/status special case
or formation variant has complete parity. Those wider behaviors remain separate port work.

Version-2 saves cover current field/actor/roster/event state. Version-1 profiles initialize a fresh Chapter 1 game. Field
autosave, SAVE and return-to-title are native platform adaptations. Active combat keeps a pre-battle checkpoint; a full
combat snapshot is not fabricated from a partial field save. Native explanatory error/result text and added save commands
are authored adaptations; original reference sources and the immutable requirements backlog remain unchanged.
