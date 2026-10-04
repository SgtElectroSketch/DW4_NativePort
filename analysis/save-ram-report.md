# Save RAM Verification

The MMC1 cartridge exposes battery-backed SRAM at `$6000-$7FFF`. Runtime tracing observed direct writes to every listed address. The game plays out of a working save at `$6001-$62EE`; recovered code reads and writes those addresses in place.

## Adventure Log slots and checksum

The working save is not the saved game. Bank `$12` keeps 3 Adventure Log records of 752 bytes starting at `$62EF` (record address = `$62EF` + slot * 752, bank `$12:$AD1C-$AD31`). Each record is a two-byte checksum followed by a 750-byte copy of the working save `$6001-$62EE`.

- **Checksum** (`$12:$AC9B`): CRC-16, polynomial `$1021`, most significant bit first, state seeded with `$3A3A`, over the 750 payload bytes at record offsets 2-751. The per-byte update is `$1F:$C8AD`, the same routine that advances the random-number state in `$12/$13`, so computing a checksum overwrites that state.
- **Write** (`$12:$ACDF`): stores the checksum little-endian in record bytes 0-1. The save service `$12:$AC53` (bank `$12` directory entry `$0E`) copies the working save into the slot and then calls it.
- **Validate** (`$12:$ACBD`): a record whose bytes 0-4 are all `$4B` is erased (`A=$01`). Otherwise the routine subtracts the stored word from the recomputed one and branches on the high-byte result only; the low-byte result is discarded and only its borrow carries over. A slot is therefore accepted (`A=$00`) when computed minus stored lies in `$0000-$00FF`, not only on exact equality, and rejected (`A=$80`) otherwise. `$12:$AA5B` erases a rejected slot.
- **Signature**: `$12:$AC73` compares the 13 ROM bytes `$12:$AD71-$AD7D` with the copies at `$6BBF` and `$6BCC`; each byte must match in at least one copy. When one does not, `$12:$AA41` rewrites both copies and fills every slot with `$4B`. The verifier applies the same test to each archived save and requires both ledger fields to span exactly those bytes.

Corroboration: 28/28 archived battery saves pass the signature test and 28/28 occupied slots store exactly the computed checksum; 56 slots are erased.

Limits: observed writes show that each byte is written, not that a slot is intact; slot integrity rests on the checksum above. Load-time behavior beyond the checksum and signature tests, and every working-save byte outside the fields below, remain undocumented here.

## Fields

| Field | Range | Runtime writes | Initialization | Evidence |
|---|---:|---:|---|---|
| SaveCharacterRecords | `$6001-$610E` | 270/270 | Initialized per chapter and updated by party progression | Nine 30-byte character records addressed by pointers $6001-$60F1 in bank $10 table $9F83; inventory occupies fields $13-$1A |
| SaveGuestCharacterRecords | `$610F-$6156` | 72/72 | Cleared by InitializeChapterSaveState and populated for guest characters | Twelve 6-byte guest-character records addressed by pointers $610F-$6151 in bank $10 table $9F83 |
| SaveTotalGold | `$6157-$6159` | 3/3 | Set by chapter initialization | 24-bit little-endian value: LDA $6157 / LDA $6158 / LDA $6159 at bank $10:$8699-$86A3 load it against the cap $01869F (99,999) from LDA #$9F / LDA #$86 / LDA #$01 at bank $10:$868D-$8695, and LSR $6159 / ROR $6158 / ROR $6157 at bank $12:$ADD5-$ADDB halve all three bytes |
| SaveCurrentChapterMinus1 | `$615A-$615A` | 1/1 | Caller-selected before chapter initialization | Chapter branches across banks $08/$10-$12/$18/$19 |
| SaveHeroGender | `$615C-$615C` | 1/1 | Initialized from chapter record | Read by map and battle presentation code |
| SaveHeroName | `$615D-$6164` | 8/8 | Initialized from chapter record | Eight-byte copy loop reads offsets $00-$07 |
| SavePartyCharacters | `$616A-$616D` | 4/4 | Initialized from chapter-specific ROM tables | Four party identifiers written by bank $12:$8ECA |
| SaveGameStateFlags | `$618E-$618E` | 1/1 | Cleared then conditionally set by chapter initialization | Tested by BIT $618E at bank $10:$824A and BIT $618E at bank $10:$8261, and written by bank $12 |
| SavePlayerWorldPosition | `$6197-$6198` | 2/2 | Updated by map position logic | Compared with PlayerWorldX/PlayerWorldY by fixed-bank map logic |
| SaveVaultGoldThousands | `$625B-$625C` | 2/2 | Vault deposit and withdrawal updates | Bank $15:$B4B9-$B4ED stores deposited gold in 1000-gold increments |
| SaveStoryFlags | `$625D-$625D` | 1/1 | Game-event updates | Bit-field operations in bank $1E |
| SaveVehicleFlags | `$628E-$628E` | 1/1 | Game-event updates | Tested by BIT $628E at bank $1E:$AB0D and BIT $628E at bank $1E:$AB3D in bank $1E movement logic |
| SaveJoinedCharacterFlags | `$6292-$6292` | 1/1 | Party-event updates | Read by bank $19 map-music selection |
| SaveTransformState | `$6296-$6297` | 2/2 | Map-event updates | Read/write use of step and shape bytes in banks $1D/$1E |
| SaveSmallMedals | `$62A2-$62A2` | 1/1 | Item-event updates | Named field used by item and event services |
| SaveCasinoCoins | `$62AD-$62AF` | 3/3 | Casino updates | Three-byte carry-propagating arithmetic in bank $17 |
| SaveTimeOfDay | `$62ED-$62ED` | 1/1 | Frame/world-time updates | Incremented and wrapped by fixed-bank time logic |
| AdventureLogSlot1 | `$62EF-$65DE` | 752/752 | Zero-filled when a log is created (bank $12:$AB36), rewritten from the working save by service $12:$AC53, and filled with $4B when erased ($12:$AC10) | 752-byte record: CRC-16 word in bytes 0-1, then a 750-byte copy of the working save $6001-$62EE; the address is $62EF + slot * 752 from LDA $AD80 / LDA $AD81 / LDA $8A / JSR $C827 / LDA $AD88 / LDY $AD89 / JMP $C81D at bank $12:$AD1C-$AD31 |
| AdventureLogSlot2 | `$65DF-$68CE` | 752/752 | Zero-filled when a log is created (bank $12:$AB36), rewritten from the working save by service $12:$AC53, and filled with $4B when erased ($12:$AC10) | 752-byte record: CRC-16 word in bytes 0-1, then a 750-byte copy of the working save $6001-$62EE; the address is $62EF + slot * 752 from LDA $AD80 / LDA $AD81 / LDA $8A / JSR $C827 / LDA $AD88 / LDY $AD89 / JMP $C81D at bank $12:$AD1C-$AD31 |
| AdventureLogSlot3 | `$68CF-$6BBE` | 752/752 | Zero-filled when a log is created (bank $12:$AB36), rewritten from the working save by service $12:$AC53, and filled with $4B when erased ($12:$AC10) | 752-byte record: CRC-16 word in bytes 0-1, then a 750-byte copy of the working save $6001-$62EE; the address is $62EF + slot * 752 from LDA $AD80 / LDA $AD81 / LDA $8A / JSR $C827 / LDA $AD88 / LDY $AD89 / JMP $C81D at bank $12:$AD1C-$AD31 |
| AdventureLogSignature | `$6BBF-$6BCB` | 13/13 | Copied from ROM $12:$AD71-$AD7D | Thirteen bytes: LDA $AD71,X / CMP $6BBF,X at bank $12:$AC75-$AC78 tests them and LDA $AD71,X / STA $6BBF,X at bank $12:$AC8E-$AC91 rewrites them |
| AdventureLogSignatureCopy | `$6BCC-$6BD8` | 13/13 | Copied from ROM $12:$AD71-$AD7D | Thirteen bytes: CMP $6BCC,X at bank $12:$AC7D accepts a byte the first copy fails, and STA $6BCC,X at bank $12:$AC94 rewrites them |
