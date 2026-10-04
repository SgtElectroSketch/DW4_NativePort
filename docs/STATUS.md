# Project Status

Last verified: 2026-10-01

This document is the authoritative human-readable status snapshot. Generated totals come from `../analysis/`; completion policy is enforced by `../verify-completion.cmd`.

## Current Metrics

- Reassemblable assembly: 100% (verified: build reproduces SHA-256 `373BE958CB33651FE599A6B282D2A232EB3B99559C258B2C70B53DF0FA31E34A`)
- Detailed semantic classification: 100% (524,288 / 524,288) - Done
- Verified instruction-byte inventory: 163,493 (31.18% of PRG) - Done under current decoded evidence
- Explicit content-range inventory: 361,313 (68.91% of PRG) - Done
- Reviewed dual-use code/data overlap ledger: 518/518 bytes (0.10%) - Done
- Curated code/function labels: 4,572
- Meaningfully named routines: 4,562/4,562 (100%)
- Semantic contracts: 4,562/4,562 (100%)
- Pointer recovery, indirect-jump audit, and analyzer-warning disposition: 100%
- Current analyzer warnings and control-flow conflicts: 0
- Structured asset encoders: 0/5 complete

The instruction, content-range, and overlap figures are classification components, not independent progress
targets. Their union is complete: 163,493 instruction bytes + 361,313 content-range bytes - 518 bytes present in
both inventories = 524,288 classified PRG bytes.

All-bank entry-point pass: 2,141 pointer entries across declared tables, mixed records, text/UI escape handlers,
and explicit pointer fields. All 2,036 executable targets decode; RTS-dispatch tables are registered with their
value+1 targets. All 38 decoded indirect jumps have reviewed dispositions. Correcting the two RTS-biased
subtables at `$16:$A73B/$A777` removed a false operand entry, and progression-state tracing recovered the
handler at `$16:$AAEF` from a stale variable-record boundary.

The completion gate (`verify-completion.cmd`) passes end to end: 0 current analyzer warnings; 215 warning
identities ledgered, including all 143 original warnings; 2,141 pointers typed; 2,036/2,036 executable targets
decoded; 38/38 indirect jumps audited; 4,562 routine interfaces; 4,562 semantic contracts; 26 asset slices; 22 save
fields; 9 runtime paths; exact ROM match. Since 2026-10-01 the gate also runs the 23 `Dw4Tool self-test` probes,
the routine-contract synchronizer in check mode, and seven save-verifier regression cases.

The 2026-09-28 routine-name audit checked all 2,354 interfaces in banks `$00-$15` against their generated ASM
bodies and interface evidence. A follow-up control-flow pass added absolute JMP trampolines and bounded tail-call
targets, increasing the all-bank inventory from 4,315 to 4,562 routines. Confirmed semantic errors, operand-derived
names, and generic subsystem prefixes were corrected. The audit now resolves direct `$D3` battle-message calls
against decoded text and flags generic selection, accumulator, marker, route, action, result, and state names on
their owning routines. It reports zero review flags; duplicate-address, duplicate-global-name, and byte-identical
fixed-bank semantic-name checks are also clean.

The follow-up audio audit decoded bank `$19` entries `$02-$09` as APU reset, track start, completion flags,
flagged track start, global audio setting, completion wait, map-track selection, and map-music playback. It also
corrected physical damage, victory rewards, monster stats, agility-based turn order, casino payouts, cursed-item
feedback, and map-event jingles. The naming report now warns on audio-only display/print/message labels and on
`Dormant` labels with direct or tail callers; both queues are empty.

The corrective field/story audit registered selector `$D3` as battle-message dispatch and `$04,$6F` as the direct
field-message service. Bank `$12:$9300-$B5FF` now distinguishes item use, field spells, Adventure Log handling,
level growth and spell learning, the Lighthouse fire scene, and neutral transition/operation helpers whose owning
subsystem is not proven. Banks `$1C-$1E` identify their evidenced story scenes; chapter labels consistently use
displayed values 1-5. All 100 changed lower-fixed names were compared with their prior semantics; stronger Select,
field-frame, Start-menu, entity visibility, map-object animation, and Coin Seller names were restored in both twins.

The fifth corrective naming pass renamed core battle routines for their main path rather than an early failure
branch; standardized `$7361-$7362` and its consumers as the active battle damage amount; and curated the battle
command menu while leaving action `$41/$43` identities explicitly unverified. Sparse generated-label overrides now
assign evidenced subsystem prefixes to address ranges before falling back to each bank's dominant classification,
correcting 335 bank `$12` and 293 bank `$17` generated definitions. Seven fly-away routines and roughly 40 field,
service, and story routines now use decoded behavior. The naming review also flags failure-framed names that print
direct battle text; the resulting review queue remains empty.

The 2026-09-29 sixth audit covered all 4,905 curated labels and manually reviewed all 4,577 Function/Code bodies.
All 126 findings (37 High, 46 Medium, 43 Low) are retained in `../analysis/audits/audit6-ledger.tsv`. Corrections
include battle-AI scoring in bank `$13`, monster display and ordered Necrosaro form transitions in bank `$14`,
shop/dialogue event scripts in bank `$15`, the field-command and found-item flows in bank `$1E`, scripted battles
and party joins, fixed-bank encounter/world-trigger semantics, and the retail-unreachable chapter-selection path.
The pass also separates CharacterRecord, BattlePartyRecord, and CombatantRecord vocabulary and corrects music,
palette, handler-table, and debug-feature data classifications. `../scripts/check_audit6_labels.py` verifies the
ledger totals and rejects stale audit names, duplicate labels, missing key mappings, and routine-contract drift.

The 2026-09-29 follow-up sweep resolved 14 residual findings (1 High, 4 Medium, 9 Low). It moved the Clay Doll
battle name to the actual `$1D:$9C52` entry, corrected character-record bit set/clear direction and the capped-add
path at `$10:$865B`, replaced unsupported tactics terminology with packed-record-field selectors, and named the
party inventory search. Canonical map identities now qualify all 82 bank `$1C` and seven bank `$1D` map-specific
handlers. The remaining corrections cover treasure/search routines, casino display restoration, the Iron Safe
return, boss-ID notes, shared RAM `$75BB`, named trampoline calls, and residual bank `$14` display terminology.

The next verification sweep found eight regressions introduced by that pass. Four adjacent bank `$1C` handlers
are now aligned with their actual Lighthouse, Kievs, Cave of Betrayal, and Keeleon table entries; Haville uses
submap one. Bank `$1E` now distinguishes the Cave of Betrayal movement effect, map-transition party helpers,
unused DOOR helper, map-exit SEARCH message, and general map/field directory. Bank `$13:$91A9` correctly records
eighteen action IDs versus seventeen real handlers, leaving action ID `$60` without a handler entry.

The final one-issue audit correction identifies `$1E:$9E7A` as `FindHeroPartyOrdinalOrFallback`, called only by
chapter startup, and limits `$1E:$8FA5` to its verified scene-transition caller.

The contract-completion pass now covers all 4,562 verified routine interfaces. It preserves 41 specialized,
hand-authored contracts and derives 4,521 evidence-bounded contracts from the reviewed all-body label notes plus
conservative decoded register, entry-flag, memory-write, call, BRK-service, and entry evidence.
`../scripts/sync_routine_contracts.py` reproduces the inventory; extraction rejects missing, stale, duplicate, or
label-mismatched contracts and validates the instruction citations of the hand-authored ones, and the gate fails
when a derived contract differs from the regenerated interface inventory.

The 2026-09-28 naming pass completed banks `$16-$1F`. Every bank now has zero generated routine-entry names in
both generated assembly and the routine-interface inventory.

## Routine Naming Completion

`Named` counts curated code/function/interrupt/vector labels that coincide with an entry in
`analysis/routine-interfaces.tsv`. `Remaining %` uses each bank's routine count as its denominator. Pure-data banks
have no routine interfaces and report `n/a`.

| Bank | Named | Total | Remaining | Remaining % |
| --- | --- | --- | --- | --- |
| `$00` | 0 | 0 | 0 | n/a |
| `$01` | 0 | 0 | 0 | n/a |
| `$02` | 0 | 0 | 0 | n/a |
| `$03` | 0 | 0 | 0 | n/a |
| `$04` | 0 | 0 | 0 | n/a |
| `$05` | 0 | 0 | 0 | n/a |
| `$06` | 0 | 0 | 0 | n/a |
| `$07` | 0 | 0 | 0 | n/a |
| `$08` | 84 | 84 | 0 | 0.00% |
| `$09` | 0 | 0 | 0 | n/a |
| `$0A` | 0 | 0 | 0 | n/a |
| `$0B` | 10 | 10 | 0 | 0.00% |
| `$0C` | 0 | 0 | 0 | n/a |
| `$0D` | 0 | 0 | 0 | n/a |
| `$0E` | 6 | 6 | 0 | 0.00% |
| `$0F` | 360 | 360 | 0 | 0.00% |
| `$10` | 392 | 392 | 0 | 0.00% |
| `$11` | 374 | 374 | 0 | 0.00% |
| `$12` | 283 | 283 | 0 | 0.00% |
| `$13` | 393 | 393 | 0 | 0.00% |
| `$14` | 196 | 196 | 0 | 0.00% |
| `$15` | 256 | 256 | 0 | 0.00% |
| `$16` | 418 | 418 | 0 | 0.00% |
| `$17` | 258 | 258 | 0 | 0.00% |
| `$18` | 92 | 92 | 0 | 0.00% |
| `$19` | 12 | 12 | 0 | 0.00% |
| `$1A` | 0 | 0 | 0 | n/a |
| `$1B` | 184 | 184 | 0 | 0.00% |
| `$1C` | 221 | 221 | 0 | 0.00% |
| `$1D` | 318 | 318 | 0 | 0.00% |
| `$1E` | 340 | 340 | 0 | 0.00% |
| `$1F` | 365 | 365 | 0 | 0.00% |

## Enforced Evidence Checks

`Dw4Tool extract` fails unless every check below holds:

- Inline-operand ABI (`config/inline-operand-abi.tsv`): BRK operand counts come from the dispatcher and handler
  code, with each rule citing its handler. No inline operand byte may be a runtime-executed instruction start.
  Stack-correlated tracing proves the declared continuation for 688 call sites; 34 older sites remain explicitly
  labeled `legacy-address-only`, not causal resume evidence. Any conflicting causal continuation fails extraction.
- Progression-state tracing stages archived `.sav`/`.fcN` files only under `work/fceux`; the 31-snapshot corpus
  covers all chapters and four final-battle forms. `analysis/fceux-read-sources.tsv` attributes each ROM read to
  its executing bank/PC, while `analysis/fceux-observations.tsv` records selected register and RAM domains.
- Runtime evidence admits only unmodified state execution. A forced record-selection experiment entered excluded
  bank `$00` dialogue data; its observations were rejected and removed before rebuilding the normal state corpus.
- BRK services `$2A,$0F` and `$2B,$0F` now resolve to bank `$10:$A240/$A256`: they select a random set-bit index
  from the low nibble or full byte, return carry clear for an empty mask, and have reviewed routine contracts.
- Evidence priority: imported Ghidra blocks are supplementary. Blocks that start inside established code, raise
  any analyzer warning, or overlap a verified content range are rejected and listed in `analysis/code-report.txt`.
- A BRK that selects a bank without a verified `$8000` service directory, or a JSR/JMP into `$0800-$5FFF`, stops
  its path as invalid code.
- `config/code-data-overlaps.tsv` must exactly equal the final code/content intersection. This preserves the 518
  reviewed dual-use bytes while rejecting any undeclared overlap, regardless of seed source.
- Warning ledger (`config/analyzer-warning-ledger.tsv`): current warnings must match exactly, and each resolved
  warning's disposition is re-checked. `config/analyzer-warning-manifest.tsv` protects the full ledger and original
  inventory, including reason text and post-original identities, against unreviewed edits or deletion.
- Evidence citations: in content-range, index-bound, entry-table, save-field, and hand-authored routine-contract
  evidence, every 6502 mnemonic must lie inside a citation `MNEMONIC operand at $ADDR` (or a slash-joined group
  over a window) that parses and names decoded code at that place. A mnemonic with a mistyped operand, a missing
  or overlong address, a broken group, lowercase spelling, or no place at all fails extraction. Indirect operands
  and BRK services (`BRK $SS,$LL at $ADDR`, checked against the operand bytes) are part of the grammar. Prose that
  describes code without naming an instruction is not checked, and other files (the inline-operand ABI, code
  seeds and exclusions, the warning ledger, label notes, derived contracts) are outside this check.
- Seed-order independence: extraction re-analyzes with the evidence seeds reversed and requires identical
  instructions, inline operands, labels, and warnings. Supplementary Ghidra blocks keep their order because each
  is accepted or rolled back against the blocks before it. Only the reversed order is tried, not every
  permutation.
- `Dw4Tool self-test` assembles 23 synthetic probes and runs the production analyzer, interface, entry-flag, and
  citation code against them, so a traversal or validation regression fails the gate even when ROM-derived counts
  are unchanged.
- Adventure Log integrity: `scripts/verify-save-ram.ps1` reads the record lengths, slot base and count, seed,
  polynomial, and signature location and length from the ROM bytes the save code loads, pins the checksum-step,
  validator, and signature-loop instruction bytes, and ties the slot and signature ledger fields to them. Every
  archived battery save must be 8,192 bytes, pass the game's signature test, and store exactly the recomputed
  checksum in each occupied slot; a malformed save fails rather than counting as absent. The archives are
  local-only; when none are present the report states that this corroboration was skipped.
  `scripts/test-save-ram-verifier.ps1` runs seven synthetic cases the verifier must accept or reject.
- Index bounds (`config/index-bounds.tsv`): 169 rows each cap one decoded absolute-indexed consumer to a cited
  index range. Extraction rejects a bound that any source-attributed runtime read or recorded index-register
  observation contradicts, and every byte a bound can reach must be typed.
- `ReviewedUnusedData` ranges are analyzed without data suppression and fail extraction if they contain decoded
  code or inline operands, overlap a code exclusion, receive a declared pointer or a static absolute/indexed
  reference, execute, or receive a source-attributed runtime read. An indexed reference counts across base through
  base+255 unless an index bound caps it. Fifty-three ranges currently satisfy this policy.
- The 127 guarded flow-recovery seeds are revalidated against a baseline decode.

The orphan audit discovered the indirect parser for `$12:$A2B6-$A303` through chapter pointers at `$916E` and
runtime reads from `$8FC9/$8FD5`. Banks `$0E:$BAD7-$BAF6`, `$13:$94EC-$951A`, and `$1D:$916F-$918D` now use the
reviewed-unused policy. Bank `$08:$8ABB-$8ADA` is now typed through its `$80C2/$80C3` pointer consumer, while
`$08:$8AA2-$8AAE` stays open because `$8AA2,Y` has no proven ceiling; this is intentionally not overridden by
negative runtime evidence.

Bank `$14` is now fully classified. The final proofs include four action-presentation records selected only by
bank `$11` action IDs `$69-$6C`, sixteen motion offsets bounded by the preceding sprite movement, phase tables
bounded through both callers of `$8D7A`, special-ID tables guarded below nine, and two eleven-entry selector rows
whose ten callers set `$0F` to zero through ten. Bank `$16` is now fully classified. Its title-menu tables are proven
from the `$B78C` slot-mask builder and UI records `$53-$55`, and scripted COPY/ERASE walks exercised all eight
slot masks. Its text-input and dialogue-row tables are proven from the only records that select them.

Corrections made in the 2026-09-27 pass:

- Three reviewed-unused claims were wrong and are now typed: `$13:$94EC-$951A`, `$10:$95EF-$9600`, and
  `$1D:$B285-$B287`.
- `$16:$B71F` was wrong as reviewed-unused. It is the last byte of the keyboard list, now `$B699-$B71F`.
- Six reviewed-unused claims were withdrawn for lacking index bounds. Five are now resolved with proofs:
  `$12:$914C-$914F` and `$12:$91D1-$91D7` are reviewed-unused under a chapter bound, `$16:$A4D6-$A4DD` is
  reviewed-unused, `$1F:$ED49-$ED4A` is the 99th audio period entry, and `$16:$BC41-$BC42` is read data (see
  the next item). At that checkpoint `$0F/$1F:$DFF0` remained open; the final classification pass below resolves it.
- `$16:$BC35` claimed six status pairs because `$10:$9613` "returns zero through six", and the `$16:$BC29/$BC2F`
  bounds used that claim. `$9613` returns zero through seven: field five (poison) gives seven. The table has seven
  pairs, `$BC35-$BC42`, and both bounds are now `$00-$0C`. Runtime never contradicted the old bound because no
  trace displayed a poisoned member there.
- The `$16:$A4CE` evidence attributed Y at `$8E75/$8E88` to `AND #$07` at `$8E53`. That instruction only compares
  against Y; the caller at `$8CEF-$8CF9` masks and counts Y.
- The names `$16:$B755` and `$16:$B785` were wrong. They are title-menu and adventure-log row counts, not party
  data.

## Completion Definition

Semantic assembly is done only when all of these conditions hold:

- Zero unclassified byte ranges. **Done.**
- Zero generated routine-entry names. **Done.**
- Complete routine contracts. **Inventory done: all 4,562 interfaces have a contract synchronized with the
  generated interface inventory. Semantic review open: the label notes that supply each derived contract's
  behavior were written against bodies that stopped at BRK services and cross-bank tail jumps, and they have not
  been re-reviewed for the 1,331 interfaces those corrections widened.**
- Structured lossless encoders for all five asset classes. **Open: 0/5.**
- Fully proven save/load and validation behavior. **Open: the Adventure Log slot layout, checksum, validator, and
  signature test are recovered and verified; the working-save bytes outside the 22 ledgered fields and the rest of
  the load path are not.**
- Deterministic end-to-end runtime scenarios for all nine domains. **Open; current trace assertions pass but are
  evidence checks rather than complete scenarios.**
- Continued exact-ROM reproduction. **Mandatory gate; currently passing.**

## Review Findings (2026-09-30) And Corrections (2026-10-01)

The 2026-09-30 review recorded nine findings (3 High, 5 Medium, 1 Low) against earlier naming, contract, and
save-integrity claims. A second review on 2026-10-01 found seven defects in the first round of corrections; they
are listed under "Second Review" below and folded into the finding they belong to. The corrections withdraw
claims rather than add coverage: the byte, routine, and contract totals are unchanged, 1,331 of 4,562 routine
interfaces gained register accesses, writes, or calls they had been missing, and the "no checksum" save claim is
retracted. One finding, DW4-R10, is open.

### DW4-R01 (High, corrected): Gold is a 24-bit field

[config/save-ram.tsv](../config/save-ram.tsv#L4) now declares `SaveTotalGold` as `$6157-$6159`, 24-bit
little-endian. Its evidence cites the instructions and extraction validates them:
`LDA $6157 / LDA $6158 / LDA $6159` at [bank $10:$8699-$86A3](../src/banks/bank_10.asm#L1020) against the
`$01869F` (99,999) cap, and `LSR $6159 / ROR $6158 / ROR $6157` at
[bank $12:$ADD5-$ADDB](../src/banks/bank_12.asm#L6472). A schema built from the old ledger would have truncated
gold above 65,535.

### DW4-R02 (High, corrected): Adventure Log checksum recovered, "no checksum" claim withdrawn

[$12:$AC9B/$ACBD/$ACDF](../config/labels.tsv#L1350) are now `AdventureLog_ComputeSlotChecksum`,
`AdventureLog_ValidateSlotChecksum`, and `AdventureLog_WriteSlotChecksum`, with hand-authored
[contracts](../config/routine-contracts.tsv#L1428). The saved game is not the working save at `$6001-$62EE`: three
752-byte records start at `$62EF`, each a checksum word followed by a 750-byte copy of the working save. The
checksum is a CRC-16 (polynomial `$1021`, most significant bit first, seed `$3A3A`) over record offsets 2-751; its
per-byte step is [$1F:$C8AD](../src/banks/bank_1F.asm#L1341), the routine that also advances the random state in
`$12/$13`, so computing a checksum overwrites that state.

The validator at [$ACBD](../src/banks/bank_12.asm#L6291) is not a full-word equality test. It returns `A=$01` for
an erased record (bytes 0-4 all `$4B`); otherwise it subtracts the stored word from the recomputed one, discards
the low-byte result, and branches on the high-byte result alone, so it accepts (`A=$00`) whenever computed minus
stored lies in `$0000-$00FF` and rejects (`A=$80`) otherwise.

[The verifier](../scripts/verify-save-ram.ps1) no longer asserts that no checksum exists. It derives the layout
and parameters from ROM bytes and applies the game's own tests to the archived battery saves: 28/28 pass the signature test and 28/28
occupied slots store exactly the computed checksum. A save of the wrong size or one the game would reject fails
the verifier. [The report](../analysis/save-ram-report.md#L5) documents the layout,
checksum, validator, signature test, and remaining limits, and the ledger gained the three slot records and both
signature copies (17 to 22 fields).

### DW4-R03 (High, corrected): Routine bodies continue past BRK services and tail jumps

[CodeAnalyzer.ResumeAddress](../tools/Dw4Tool/CodeAnalyzer.cs#L461) is the single definition of where a body
continues after a BRK service or inline-operand JSR; the analyzer and the interface walk both use it. The body
has one [stated definition](../tools/Dw4Tool/Program.cs#L2472): every decoded instruction reachable from the
entry through fallthrough, branches, and direct JMPs into any bank the target resolves to. JSR callees and BRK
services are not entered; they are listed. A jump that cannot be resolved ends its path and is listed.

Against the previous inventory, 1,331 interfaces changed (1,008 gained direct writes, 897 gained calls); none
lost an entry. [EnterMapAtWorldCoordinates](../config/routine-contracts.tsv#L48) now lists its post-BRK writes
and calls, and `UploadResolvedTileGraphics` now includes the `$1F` write of its tail jump to `$0F:$C62D`. Two
new columns, `BrkServices` (1,520 routines) and `UnfollowedJumps` (120 routines), record what the body leaves
to code outside it. Contracts were regenerated (2,508 rows changed), and the gate fails when a derived contract
differs from the regenerated interface inventory.

### DW4-R04 (Medium, corrected): Analyzer reachability does not depend on seed order

Each followed instruction records the facts it was walked under (known zero flag, preceding branch) per walk
window, and is [walked again](../tools/Dw4Tool/CodeAnalyzer.cs#L267) unless an unbounded walk, or a walk of the
same window, already covered the arriving facts. A branch edge pruned for one entry is therefore still explored
for another, a bounded window no longer hides code from a later unbounded entry, and two windows that start at
one address are both walked. Four self-test probes cover the two reported flag cases and the two window cases in
both seed orders.

Effect on the current ROM, now quantified: no instruction, inline operand, or warning changed. One branch target
gained a label: [$17:$A119](../src/banks/bank_17.asm#L3992), the target of `BEQ` at `$A115` after `LDY $C000`.
ROM `$C000` is `$FF`, so the first walk pruned that edge; a Ghidra block entry at `$A115` arrives without that
fact. Extraction [re-analyzes with the evidence seeds reversed](../tools/Dw4Tool/CodeAnalyzer.cs#L505) and
requires identical results.

### DW4-R05 (Medium, corrected): Entry flags are traced, with the unknowns stated

`analysis/routine-interfaces.tsv` has two columns produced by
[EntryFlagAnalyzer](../tools/Dw4Tool/EntryFlagAnalyzer.cs):

- `EntryFlagReads`: flags (C, N, V, Z) that some decoded path consumes before any decoded instruction writes
  them. Direct JSR and JMP targets are followed through per-entry summaries, PHP/PLP are matched on a modeled
  stack, a call that never returns to its call site ends the path, and a branch on a flag the path itself set
  to a constant follows only its feasible edge. Eighty-four routines read an entry flag (47 carry, 29 zero,
  7 negative, 1 carry and negative).
- `EntryFlagsUnresolved`: flags that still held the caller's value where a path left what the trace can follow:
  a BRK service (carry and overflow only, since the dispatcher rewrites N and Z before the service runs), an
  unresolved jump or call, undecoded code, a return that does not go back to the call site, or a stack-page or
  stack-pointer access while saved flags are on the modeled stack. 1,877 routines have at least one.

For the remaining 2,627 routines every flag is written on every decoded path before any decoded read. Neither
column is a bound on the true dependencies: a listed read may lie on a path the program never takes (only
constant-flag branches are pruned), and nothing behind an unresolved flag is examined. The earlier statement
that the read set was a lower bound is withdrawn.

[$10:$8C58](../src/banks/bank_10.asm#L2032) is renamed `ReturnCarryWhenIncomingZeroFlagClear`; its hand-authored
[contract](../config/routine-contracts.tsv#L579) records that it never reads A and cites the `AND` that
immediately precedes each of its seven calls.

### DW4-R06 (Medium, corrected): Every mnemonic in evidence text must be a checked citation

[CitationValidator](../tools/Dw4Tool/CitationValidator.cs) no longer tries to recognize malformed citations. It
enforces one invariant: in validated evidence, every 6502 mnemonic must lie inside a citation that parses and
names decoded code at the cited place. The grammar covers indirect operands and BRK services with their operand
bytes. A probe checks that `LDA ($09), Y at $8FD5`, a malformed group prefix, an overlong or short address, a
lowercase citation, and a mnemonic with no place are all rejected.

Bringing the evidence under the invariant rewrote 227 rows: 196 content ranges, 23 index bounds, and 8 entry
tables, plus three hand-authored contracts and two save fields. Instructions named without an address were given
one, checked against the assembly; arithmetic and nouns that used a mnemonic word were reworded. Two rows were
factually wrong and are corrected: `$12:$8741` is guarded by `CMP #$04` at `$86EE`, not `CPX #$04`, and
`$1C:$BDD3` holds the operands of BRK service `$62,$23` at `$BDD2`, not `$62,$33`.

The invariant covers the five validated sources only. Evidence text in the inline-operand ABI, code seeds and
exclusions, the warning ledger, label notes, and derived contracts is not parsed, and prose that describes code
without naming an instruction is never a citation.

### DW4-R07 (Medium, corrected): Adventure Log byte counts are named as counts

[$12:$AD34/$AD3F](../config/labels.tsv#L1357) are now `AdventureLog_LoadPayloadByteCount` (750, `$AD7E`) and
`AdventureLog_LoadRecordByteCount` (752, `$AD80`). Their contracts state that `$02/$03` is the countdown word
`$AD03` decrements to zero, while the record address stays in `$00/$01`.

### DW4-R08 (Medium, corrected): Adventure Log creation initializes nine character records

[$12:$AB36](../config/labels.tsv#L1339) is now `AdventureLog_CreateSlotAndInitializeCharacters`.
[The loop at $ABA4](../src/banks/bank_12.asm#L6120) runs X from 8 through 0, nine iterations, setting byte 5 of
each of the nine 30-byte character records (`$6001-$610E`) to 1. Those are character records in the working save, not
Adventure Log slots; the contract separates the two.

### DW4-R09 (Low, corrected): Overlap total

`Enforced Evidence Checks` now gives 518 reviewed dual-use bytes, matching `config/code-data-overlaps.tsv`, the
classification report, and `Current Metrics`.

### DW4-R10 (Medium, open): Tail-jump discovery still stops at BRK

Found while correcting DW4-R03. [BuildRoutineTargets](../tools/Dw4Tool/Program.cs#L2442) discovers tail-jump
routine entries with a walk that still ends at a BRK. Continuing past returning services surfaces 81 further jump
targets, which would raise the inventory from 4,562 to 4,643. They are not all routine entries: 26 are reached
only by a backward jump from inside the routine that contains them (for example the search loop head at
`$08:$AEE8`) and 6 only by a forward jump inside the same routine. The walk was left unchanged rather than adding 81 entries that would need names the
evidence does not yet support. **Follow-up:** make the tail-jump rule distinguish a loop or join from a tail call,
then review and name the entries that remain.

### Second Review (2026-10-01)

A review of the first corrections found seven defects. Each was reproduced with a failing probe or case before
it was fixed.

- **Entry-flag "lower bound" claim (Medium).** False: the trace followed infeasible branches, trusted a restored
  flag after a stack-page write, and continued after calls that never return. The claim is withdrawn, the three
  defects are fixed, and unknowns are now reported per routine (DW4-R05).
- **Unchecked citations (Medium).** The shape heuristic missed several malformed forms. It is replaced by the
  mnemonic invariant (DW4-R06).
- **Signature integrity described, not enforced (Medium).** The save verifier accepted a save with both signature
  copies corrupted and a shortened signature ledger span. It now applies the game's signature test and ties both
  ledger fields to the compared bytes (DW4-R02).
- **Malformed saves treated as absent (Medium).** An 8,191-byte save produced a passing "no saves" result. Any
  present save of the wrong size now fails.
- **Bounded-window ordering (Medium).** Two windows from one address, or a window and an unbounded entry, decoded
  different code depending on order. Followed-instruction facts are now kept per window (DW4-R04).
- **Cross-bank tail jumps dropped (Medium).** Interfaces followed same-bank jumps only. The body definition now
  includes direct jumps into the fixed bank and lists what it does not enter (DW4-R03).
- **Completion wording (Low).** "Complete reviewed routine contracts: Done" overstated the review. The
  completion definition now separates the synchronized inventory from the open semantic re-review.

### Correction Verification And Limits

- Full `verify-completion.cmd` run on 2026-10-01: passed, exact ROM match, 0 analyzer warnings, 23/23 self-test
  probes, 7/7 save-verifier cases, contracts synchronized (41 hand-authored, 4,521 derived), 22 save fields with
  complete write evidence.
- `scripts/check_audit6_labels.py` passes: 4,914 labels, no duplicate or legacy names, contracts match labels.
- Not done: no new manual audit of all 4,562 routine bodies. The 1,331 widened interfaces changed derived
  register, write, call, and service lists only; their label notes were not re-reviewed against the longer bodies.
- Entry-flag columns are not bounds (see DW4-R05). The citation invariant covers five evidence sources (see
  DW4-R06). Seed-order independence is checked for the reversed order only.
- The interface lists BRK services by operand bytes; it does not resolve them to service routines or include
  their effects.
- The entry-table reason for `$12:$AACA` still calls the five Adventure Log operation handlers a "battle setup
  handler table"; that text was noticed and left unchanged.
- No fresh emulator traces were recorded. The save corroboration uses the existing local save archives.

## Remaining Work

1. Build structured lossless decoders and encoders for text, maps, graphics, palettes, and audio.
2. Define engine-neutral game-state and content schemas from the completed assembly and asset models.
3. Implement a headless deterministic simulation and compare combat, movement, events, RNG, and save/load behavior
   against emulator traces.
4. Turn the nine passing runtime-evidence domains into deterministic end-to-end scenarios.
5. Build one vertical slice in the chosen engine, then add editing tools and intentional gameplay changes.

Entry-point recovery, byte classification, warning disposition, routine naming, routine contracts, and the exact-ROM
gate are maintenance checks now; rerun them whenever new evidence changes code or data boundaries. The one open
review finding is DW4-R10 (tail-jump discovery), which may add routine entries that then need names and contracts.

## Control-Flow Conflicts

There are no current conflicts. The three overlaps once audited as intentional were all decoding artifacts:

- Bank `$10:$BBEB`: a phantom `EOR #$A5` from reading the three-operand BRK at `$BBE7` as two-operand.
- Bank `$16:$B84A`: entered by `BPL $B84A` decoded from RTS-dispatch value `$B89F`; the handler begins at `$B8A0`.
- Bank `$1F:$CE50`: `CPY #$AA` came from a Ghidra block starting inside `JSR $CEA9`. The fixed-bank BRK
  continuation decodes `JSR $CEA9; JMP $C010`.

Each is recorded as resolved in `config/analyzer-warning-ledger.tsv`.
