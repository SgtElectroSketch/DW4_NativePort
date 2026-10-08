from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from reference.tools.AssetExtract.dw4assets import entities, maps, nes, png, sprites, monsters, monster_stats
from reference.tools.AssetExtract.dw4assets.nesbus import Machine
from reference.tools.AssetExtract.dw4assets.rom import Rom


class Patterns:
    def __init__(self) -> None:
        self.ids: dict[bytes, int] = {}
        self.tiles: list[list[int]] = []

    def tile(self, data: bytes) -> int:
        if data not in self.ids:
            if len(self.tiles) >= 8192:
                raise ValueError("opening patterns exceed the native atlas limit")
            self.ids[data] = len(self.tiles)
            self.tiles.append(nes.decode_tile_2bpp(data))
        return self.ids[data]

    def write(self, path: Path) -> None:
        height = ((len(self.tiles) + 15) // 16) * 8
        image = [0] * (128 * height)
        for number, tile in enumerate(self.tiles):
            nes.blit_tile(image, 128, tile, (number % 16) * 8, (number // 16) * 8)
        rgba = [(pixel * 85, pixel * 85, pixel * 85, 255) for pixel in image]
        png.write_rgba(str(path), 128, height, rgba)


def colors(values: list[int], transparent: bool = False) -> list[list[int]]:
    return [list(nes.rgb(value)) + [0 if transparent and index == 0 else 255]
            for index, value in enumerate(values)]

def growth_value(rom: Rom, character: int, kind: int, level: int) -> tuple[int, int]:
    hp = kind == 6
    actual_kind = 3 if hp else kind
    stride = 5 if kind == 0 else 6
    pointer = rom.word(0x12, 0xA0FB + actual_kind * 2) + character * stride
    descriptor = list(rom.bytes(0x12, pointer, stride))
    initial = sum(((descriptor[index] >> 7) & 1) << index for index in range(5))
    band = ((descriptor[0] & 0x60) >> 5) * 6 + (24 if kind else 0)
    limits = [descriptor[0] & 0x1F] + [value & 0x7F for value in descriptor[1:5]]
    cursor = 0
    total = initial
    delta = descriptor[5] if kind else initial
    first_level = 2 if kind else 3
    for current_level in range(first_level, level + 1):
        while cursor < 5 and limits[cursor] < current_level:
            cursor += 1
        if kind:
            delta = descriptor[5]
        multiplier = rom.byte(0x12, 0xA259 + band + cursor)
        delta = (delta * multiplier + (8 if kind else 0)) >> 4
        total += delta
        if total > 0xFFFFFF:
            raise ValueError("growth curve exceeded the original 24-bit bound")
    if hp:
        total *= 2
        delta *= 2
    return total, delta


def progression(rom: Rom) -> list[dict]:
    result = []
    for character in range(9):
        if character == 8:
            source = 0
        else:
            source = character
        levels = []
        for level in range(1, 100):
            experience = 0 if level == 1 else growth_value(rom, source, 0, level)[0]
            stats = [growth_value(rom, source, kind, level)[0] for kind in range(1, 8)]
            gains = [0 if level == 1 else growth_value(rom, source, kind, level)[1] for kind in range(1, 8)]
            levels.append({"level": level, "experience": experience, "targets": stats, "gains": gains})
        result.append({"id": character, "levels": levels})
    return result

def verify_growth(rom: Rom) -> None:
    for level in (2, 3, 10, 99):
        for kind in (0, 1, 3, 6):
            machine = Machine(rom)
            machine.bus.select(0x12)
            machine.bus.ram[9:12] = bytes((6, level, kind))
            machine.cpu.pc = 0x9F7C
            machine.cpu.s = 0xFF
            machine.cpu.push16(0x5FFF)
            for _ in range(200000):
                if machine.cpu.pc == 0x6000:
                    break
                machine.cpu.step()
            else:
                raise ValueError("growth verification exceeded its instruction bound")
            actual = (int.from_bytes(machine.bus.ram[0:3], "little"), int.from_bytes(machine.bus.ram[4:7], "little"))
            expected = growth_value(rom, 6, kind, level)
            if actual != expected:
                raise ValueError("growth decoder mismatch at level %d kind %d: %r != %r" % (level, kind, actual, expected))
    print("growth decoder matches 16 original routine samples")

def interaction_program(rom: Rom, selector: int) -> dict:
    if selector < 7:
        return {"steps": [{"kind": "shop", "mode": selector}]}
    if selector >= 0x161:
        return {"steps": [{"kind": "say", "message": selector + 0x39F}]}
    address = rom.word(0x15, 0x9642 + (selector - 7) * 2)
    cursor = 0
    budget = 0
    callbacks: set[int] = set()

    def byte() -> int:
        nonlocal cursor, budget
        if cursor >= 255 or budget >= 1024:
            raise ValueError("interaction program exceeds its decode bound")
        value = rom.byte(0x15, address + cursor)
        cursor += 1
        budget += 1
        return value

    def peek() -> int:
        return rom.byte(0x15, address + cursor)

    def sequence(depth: int, block: bool = True) -> list[dict]:
        if depth > 16:
            raise ValueError("interaction nesting exceeds its bound")
        result = []
        while True:
            opcode = byte()
            if opcode in (0x3E, 0x3F):
                return result
            if opcode == 6:
                result.extend(sequence(depth + 1))
            elif opcode & 0x40:
                result.append({"kind": "say", "message": ((opcode & 0x3F) << 8) | byte()})
            elif opcode == 7:
                condition = byte()
                kind = condition & 0x3F
                count = 2 if kind in (0x1A, 0x1B, 0x1C) else 1 if kind in (3, 0x11, 0x13, 0x16) else 0
                arguments = [byte() for _ in range(count)]
                many = peek() == 6
                if many:
                    byte()
                yes = sequence(depth + 1, many)
                no: list[dict] = []
                if peek() == 0x0B:
                    byte()
                    many = peek() == 6
                    if many:
                        byte()
                    no = sequence(depth + 1, many)
                result.append({"kind": "branch", "condition": kind, "invert": bool(condition & 0x80),
                               "arguments": arguments, "yes": yes, "no": no})
            elif opcode in (0x17, 0x18, 0x19):
                result.append({"kind": "flags", "operation": opcode - 0x17, "index": byte(), "mask": byte()})
            elif opcode == 0x12:
                result.append({"kind": "shop", "mode": byte()})
            elif opcode in (0, 1):
                callback = byte() | (byte() << 8)
                callbacks.add(callback)
                arguments = [byte(), byte()] if opcode == 1 else []
                semantic = {0x9D2A: "sell_counter", 0x9D36: "purchase_animation", 0xA4B6: "refresh_actors"}.get(callback)
                if semantic is None:
                    raise ValueError("actor callback requires reviewed native semantics: 15:%04X" % callback)
                result.append({"kind": "callback", "action": semantic, "arguments": arguments})
            else:
                raise ValueError("unsupported interaction decode opcode %02X at 15:%04X" % (opcode, address + cursor - 1))
            if not block:
                return result

    return {"steps": sequence(0), "source": "15:%04X" % address,
            "callbacks": ["%04X" % value for value in sorted(callbacks)]}

def item_records(rom: Rom, asset_root: Path) -> list[dict]:
    names = json.loads((asset_root / "text/names.json").read_text(encoding="utf-8"))["groups"]["items"]["names"]
    thresholds = (0x24, 0x3D, 0x46, 0x50)
    result = []
    for index in range(127):
        category = next((slot + 1 for slot, limit in enumerate(thresholds) if index < limit), 0)
        result.append({"id": index, "name": names[index]["text"].strip(),
                       "price": (rom.byte(0x10, 0x8DE2 + index) & 0x7F) * 10 ** (rom.byte(0x10, 0x8CE4 + index) & 3),
                       "slot": category, "power": rom.byte(0x10, 0x9DE0 + index),
                       "equip_mask": rom.byte(0x10, 0x8C65 + index),
                       "cursed": category != 0 and bool(rom.byte(0x10, 0x8D63 + index) & 0x80)})
    return result

def shop_records(rom: Rom) -> list[dict]:
    result = []
    starts = [rom.word(0x18, 0x802C + index * 2) for index in range(3)]
    for mode in (1, 2, 3):
        pointer = starts[mode - 1]
        end = min((value for value in starts if value > pointer), default=0xB5C2)
        for _ in range(256):
            if pointer >= end:
                break
            identity = rom.byte(0x18, pointer)
            pointer += 1
            if identity == 255:
                break
            floor = rom.byte(0x18, pointer)
            pointer += 1
            stock = []
            for _ in range(8):
                value = rom.byte(0x18, pointer)
                pointer += 1
                stock.append(value & 127)
                if value & 128:
                    break
            else:
                raise ValueError("shop stock exceeded eight entries")
            if identity == 2:
                result.append({"map": identity, "submap": floor, "mode": mode, "items": stock})
        else:
            raise ValueError("shop stock table exceeded its record bound")
    return result

def inn_records(rom: Rom) -> list[dict]:
    pointer = rom.word(0x18, 0x800E)
    result = []
    for index in range(128):
        address = pointer + index * 3
        identity = rom.byte(0x18, address)
        if identity == 255:
            return result
        result.append({"map": identity, "submap": rom.byte(0x18, address + 1), "price": rom.byte(0x18, address + 2)})
    raise ValueError("inn price table exceeded its bound")

def encounter_records(rom: Rom) -> dict:
    grid = list(rom.bytes(0x18, rom.word(0x18, 0xA241), 256))
    base = rom.word(0x18, 0xA239)
    groups = []
    for identity in range(64):
        values = list(rom.bytes(0x18, base + identity * 16, 16))
        offset = ((values[0] >> 2) & 7) * 18
        row = list(rom.bytes(0x18, 0xA28D + offset, 18))
        groups.append({"id": identity, "rate": rom.byte(0x18, 0xA340 + (values[0] >> 5)),
                       "entries": values[2:], "weights": row[:14], "extra": row[14:],
                       "control": values[1], "count_codes": row[16:]})
    mixed = []
    base = rom.word(0x18, 0xA237)
    for identity in range(256):
        values = list(rom.bytes(0x18, base + identity * 6, 6))
        mixed.append(values)
    return {"grid": grid, "groups": groups, "mixed": mixed,
            "terrain_rates": list(rom.bytes(0x18, 0xA27B, 8)),
            "first_steps": list(rom.bytes(0x18, 0xA33D, 3)),
            "mixed_span": list(rom.bytes(0x18, 0xA31D, 8)), "mixed_base": list(rom.bytes(0x18, 0xA325, 8)),
            "single_span": list(rom.bytes(0x18, 0xA32D, 8)), "single_base": list(rom.bytes(0x18, 0xA335, 8))}


def battle_monsters(rom: Rom, patterns: Patterns) -> list[dict]:
    item_names, monster_names = monster_stats.name_lists(rom)
    result = []
    for identity in range(195):
        stats = monster_stats.decode(rom, identity, item_names, monster_names)
        graphics = rom.byte(0x14, monsters.ID_MAP + identity)
        scene = monsters.compose(rom, graphics)
        draws = []
        palettes = []
        palette_ids: dict[tuple[int, int], int] = {}
        for group in ("bg", "sprite"):
            for number, values in enumerate(scene["palettes"][group]):
                palette_ids[(0 if group == "bg" else 1, number)] = len(palettes)
                palettes.append(colors([15] + values, True))
        for (column, row), (chunk, variant, palette) in sorted(scene["bg"].items()):
            tile = monsters.tile_variants(scene["chunks"][chunk]["tile"])[variant]
            draws.append([patterns.tile(tile), column * 8 + scene["origin"][0], row * 8 + scene["origin"][1], palette_ids[(0, palette)], 0])
        for chunk, entry in scene["sprites"]:
            draws.append([patterns.tile(scene["chunks"][chunk]["tile"]), entry["x"] + scene["origin"][0],
                          entry["y"] + scene["origin"][1], palette_ids[(1, entry["palette"])],
                          (1 if entry["flip_h"] else 0) | (2 if entry["flip_v"] else 0)])
        result.append({"id": identity, "name": monster_names[identity], "hp": stats["hp"], "mp": stats["mp"],
                       "attack": stats["attack"], "defense": stats["defense"], "agility": stats["agility"],
                       "experience": stats["exp"], "gold": stats["gold"], "drop": stats["drop"],
                       "actions": stats["actions"]["weighted"], "width": scene["width"], "height": scene["height"],
                       "palettes": palettes, "draws": draws})
    return result


def map_connections(rom: Rom, records: list[dict]) -> None:
    pointer = rom.word(8, 0xB974)
    for _ in range(73):
        identity = rom.byte(8, pointer)
        pointer += 1
        if identity == 0xFF:
            break
        if identity == 2:
            break
        for _ in range(4096):
            value = rom.byte(8, pointer)
            pointer += 1
            if value == 0xFF:
                break
        else:
            raise ValueError("map connection table exceeded its scan bound")
    else:
        raise ValueError("Burland connection table is absent")
    behaviors = []
    for address in range(0xB675, 0xB67D):
        value = rom.byte(8, address)
        if value == 0:
            break
        behaviors.append(value)
    destinations = []
    for submap in range(4):
        floor = []
        for _ in range(64):
            header = rom.byte(8, pointer)
            size = 3 if header & 0x60 == 0x20 else 2 if header & 0x60 in (0x40, 0x60) else 1
            floor.append(list(rom.bytes(8, pointer, size)))
            pointer += size
            if header & 0x80:
                break
        else:
            raise ValueError("submap connection list exceeded its bound")
        destinations.append(floor)
    entrances = []
    for record in records[:4]:
        entrances.append([(index % record["width"], index // record["width"])
                          for index, cell in enumerate(record["cells"])
                          if record["tiles"][cell & 31]["behavior"] & 0x7F in behaviors])
    routing = rom.word(0x0E, 0x8004)
    world_position = None
    for index in range(256):
        address = routing + index * 3
        if rom.byte(0x0E, address) == 2:
            world_position = (rom.byte(0x0E, address + 1), rom.byte(0x0E, address + 2))
            break
        if rom.byte(0x0E, address) == 0xFF:
            break
    if world_position is None:
        raise ValueError("Burland world routing record is absent")
    for submap, record in enumerate(records[:4]):
        connections = []
        for ordinal, (x, y) in enumerate(entrances[submap]):
            if ordinal >= len(destinations[submap]):
                raise ValueError("Burland behavior ordinal has no destination record")
            target = destinations[submap][ordinal]
            mode = target[0] & 0x60
            if mode == 0:
                connections.append({"x": x, "y": y, "map": 255, "submap": 0,
                                    "destination_x": world_position[0], "destination_y": world_position[1]})
            elif mode == 0x60:
                floor = target[0] & 31
                if target[1] >= len(entrances[floor]):
                    raise ValueError("Burland destination behavior ordinal is invalid")
                destination_x, destination_y = entrances[floor][target[1]]
                connections.append({"x": x, "y": y, "map": 2, "submap": floor,
                                    "destination_x": destination_x, "destination_y": destination_y})
            else:
                raise ValueError("Burland connection requires reviewed cross-map semantics")
        record["connections"] = connections
        if submap == 0:
            record["boundary_destination"] = {"map": 255, "submap": 0, "x": world_position[0], "y": world_position[1]}
    for index in range(64):
        address = 0xB681 + index * 9
        if rom.byte(8, address) == 255:
            break
        values = list(rom.bytes(8, address, 9))
        if values[0] == 2 and values[4] == 2:
            records[values[1]]["connections"].append({"x": values[2], "y": values[3], "map": values[4], "submap": values[5],
                                                     "destination_x": values[6], "destination_y": values[7]})
    routing_header = rom.word(8, 0xB7F7)
    for _ in range(73):
        if rom.byte(8, routing_header) in (2, 255):
            break
        routing_header += 5
    if rom.byte(8, routing_header) != 2:
        raise ValueError("Burland arrival header is absent")
    records[-1]["connections"] = [{"x": world_position[0], "y": world_position[1], "map": 2,
                                   "submap": rom.byte(8, routing_header + 1) & 31,
                                   "destination_x": rom.byte(8, routing_header + 2),
                                   "destination_y": rom.byte(8, routing_header + 3)}]
    records[1]["boundary_destination"] = {"map": 2, "submap": 0, "x": 19, "y": 10}

def treasure_records(rom: Rom, records: list[dict]) -> list[dict]:
    values = rom.word(0x1E, 0xBDC0)
    offsets = rom.word(0x1E, 0xBDBE)
    result = []
    for record in records[:4]:
        base = 0
        for index in range(256):
            address = offsets + index * 3
            identity, floor, count = rom.bytes(0x1E, address, 3)
            if identity > 2 or identity == 2 and floor >= record["submap"]:
                break
            base += count
        else:
            raise ValueError("treasure offset scan exceeded its bound")
        ordinal = 0
        for index, cell in enumerate(record["cells"]):
            if record["tiles"][cell & 31]["behavior"] != 4:
                continue
            value = rom.byte(0x1E, values + base + ordinal)
            identity = base + ordinal
            result.append({"map": 2, "submap": record["submap"], "x": index % record["width"], "y": index // record["width"],
                           "flag": 4096 + identity, "item": value if value < 128 else None,
                           "gold": (value & 127) * 40 if value >= 128 and value not in (255, 254, 253, 239, 238, 227, 226, 224) else 0,
                           "empty": value == 255, "reference_value": value})
            ordinal += 1
    return result


def actors(rom: Rom, submap: int) -> list[dict]:
    address = rom.word(5, 0x8004)
    for _ in range(submap):
        address = entities.walk_list(rom, 5, address, {}, [])
    result = []
    for index in range(128):
        flags = rom.byte(5, address)
        if flags == 0:
            return result
        record = rom.bytes(5, address, entities.record_size(flags))
        end = address + len(record)
        scripts = []
        for bit in (0x10, 0x08):
            if flags & bit:
                length = rom.byte(5, end)
                scripts.append({"address": end + 1, "bytes": list(rom.bytes(5, end + 1, length))})
                end += length + 1
        if flags & 0x10:
            sprite = next(value for value in entities.entity_sprites(record) if value["period"] == "day")
            patrol = []
            follow_player = False
            code = scripts[0]["bytes"]
            cursor = 0
            while cursor < len(code):
                opcode = code[cursor]
                if opcode == 0x24 and cursor + 2 < len(code):
                    if code[cursor + 1] & 128:
                        follow_player = True
                    else:
                        patrol.append([code[cursor + 1], code[cursor + 2]])
                    cursor += 3
                elif opcode in (0x23, 0x22, 0x82, 0x8A, 0x2D):
                    cursor += 2
                elif opcode in (0x26, 0x84):
                    break
                elif opcode == 0x27:
                    cursor += 4
                elif opcode == 0x83:
                    break
                else:
                    cursor += 1
            result.append({"id": index, "x": record[5] & 0x7F, "y": record[6] & 0x7F,
                           "facing": record[3] & 3, "sprite": sprite["sprite_set"],
                           "message": ((record[1] & 0x1C) << 6) | record[4], "motion": record[2] >> 4,
                           "interaction": interaction_program(rom, ((record[1] & 0x1C) << 6) | record[4]),
                           "source": "05:%04X" % address, "script": scripts[0]})
            result[-1]["patrol"] = patrol
            result[-1]["follow_player"] = follow_player
            result[-1]["remove_at_end"] = len(code) >= 2 and code[-2:] == [8, 0x2F]
        address = end
    raise ValueError("opening entity list exceeds its bound")


def export_maps(rom: Rom, output: Path) -> dict:
    asset_root = output.parent
    patterns = Patterns()
    records = []
    sprite_ids = {6}
    for submap, record in enumerate(maps.submap_records(rom, 2)):
        existing = asset_root / "maps/locations/02-burland" / ("02-%02X.json" % submap)
        data = json.loads(existing.read_text(encoding="utf-8"))
        entries = maps.slot_entries(rom, record["tileset"])
        tiles = []
        for entry in entries:
            definition = maps.slot_definition_index(entry, chapter=1)
            if definition is None:
                tiles.append({"quadrants": [patterns.tile(bytes(16))] * 4, "palette": 0, "behavior": 128})
                continue
            sources = maps.tile_sources(rom, definition)
            tiles.append({"quadrants": [patterns.tile(rom.bytes(bank, source, 16)) for bank, source in sources],
                          "palette": (entry[0] >> 3) & 3, "behavior": rom.byte(8, 0xA80D + definition * 3 + 2)})
        people = actors(rom, submap)
        sprite_ids.update(actor["sprite"] for actor in people)
        records.append({"map": 2, "submap": submap, "width": data["width"], "height": data["height"],
                        "cells": [cell for row in data["cells"] for cell in row], "border": data["border_tile"],
                        "palettes": [colors(values) for values in data["palettes"]["day"]],
                        "tiles": tiles, "actors": people})
    world = json.loads((asset_root / "maps/world/main.json").read_text(encoding="utf-8"))
    entries = maps.slot_entries(rom, 0)
    tiles = []
    for entry in entries:
        definition = maps.slot_definition_index(entry, chapter=1)
        if definition is None:
            tiles.append({"quadrants": [patterns.tile(bytes(16))] * 4, "palette": 0, "behavior": 128})
        else:
            tiles.append({"quadrants": [patterns.tile(rom.bytes(bank, source, 16)) for bank, source in maps.tile_sources(rom, definition)],
                          "palette": (entry[0] >> 3) & 3, "behavior": rom.byte(8, 0xA80D + definition * 3 + 2)})
    records.append({"map": 255, "submap": 0, "width": world["width"], "height": world["height"],
                    "cells": [cell for row in world["cells"] for cell in row], "border": 0,
                    "palettes": [colors(values) for values in world["palette_colors"]], "tiles": tiles, "actors": []})
    map_connections(rom, records)
    sprite_records = []
    for sprite in sorted(sprite_ids):
        frames = []
        for frame in sprites.sprite_set(rom, sprite):
            frames.append({"quadrants": [patterns.tile(sprites.read_cpu(rom, frame["bank"], source, 16))
                                          if source else patterns.tile(bytes(16)) for source in frame["sources"]],
                           "palette": frame["palette"], "flip_h": frame["flip_h"]})
        sprite_records.append({"id": sprite, "frames": frames})
    enemies = battle_monsters(rom, patterns)
    patterns.write(output / "patterns.png")
    return {"schema_version": 1, "image": "patterns.png", "tile_count": len(patterns.tiles),
            "maps": records, "sprites": sprite_records,
            "progression": progression(rom),
            "items": item_records(rom, asset_root),
            "shops": shop_records(rom),
            "inns": inn_records(rom),
            "encounters": encounter_records(rom), "monsters": enemies,
            "treasures": treasure_records(rom, records),
            "sprite_palettes": [colors(values, True) for values in sprites.default_palettes(rom)]}


def observe_opening(rom: Rom, output: Path) -> dict:
    machine = Machine(rom)
    machine.bus.chr0 = 0x10
    machine.cpu.reset()
    messages: list[dict] = []
    text_messages: list[dict] = []
    music: list[dict] = []
    actor_samples: list[dict] = []
    field_start: int | None = None
    last_window: tuple[int, int] | None = None
    inputs: list[tuple[int, int]] = []
    transitions: list[dict] = []
    previous_location: tuple[int, int] | None = None
    create_stage = 0
    menu_frames = 0
    chapter_card_seen = False
    chapter_card: dict | None = None

    def window(cpu) -> None:
        nonlocal last_window
        if machine.bus.bank_at(cpu.pc) == 0x16:
            last_window = (machine.frames, machine.bus.ram[0xF6])

    def message(cpu) -> None:
        nonlocal field_start
        if machine.bus.bank_at(cpu.pc) == 0x1F:
            messages.append({"frame": machine.frames, "a": cpu.a, "x": cpu.x,
                             "map": machine.bus.ram[0x63], "submap": machine.bus.ram[0x64]})
            if machine.bus.ram[0x63] == 2 and machine.bus.ram[0x64] == 1 and field_start is None:
                field_start = machine.frames

    def text_message(cpu) -> None:
        if machine.bus.bank_at(cpu.pc) == 0x16:
            text_messages.append({"frame": machine.frames, "group": cpu.a, "index": cpu.x,
                                  "map": machine.bus.ram[0x63], "submap": machine.bus.ram[0x64]})

    def music_request(cpu) -> None:
        if machine.bus.bank_at(cpu.pc) == 0x1F and cpu.a < 128:
            music.append({"frame": machine.frames, "track": cpu.a,
                          "map": machine.bus.ram[0x63], "submap": machine.bus.ram[0x64]})

    def chapter_ready(cpu) -> None:
        nonlocal chapter_card_seen
        if machine.bus.bank_at(cpu.pc) == 0x1B:
            chapter_card_seen = True

    machine.cpu.hooks[0x8AA5] = window
    machine.cpu.hooks[0xD1FD] = message
    machine.cpu.hooks[0x872B] = text_message
    machine.cpu.hooks[0xEF5B] = music_request
    machine.cpu.hooks[0xA745] = chapter_ready

    def capture(current: Machine) -> None:
        nonlocal create_stage, menu_frames, previous_location, chapter_card
        if chapter_card_seen and chapter_card is None and current.bus.ppumask & 8:
            from reference.tools.AssetExtract.dw4assets import ppu_render
            from importlib.util import module_from_spec, spec_from_file_location
            specification = spec_from_file_location("native_title_export", ROOT / "scripts/port/extract-native-title.py")
            if specification is None or specification.loader is None:
                raise ValueError("title pattern exporter is unavailable")
            module = module_from_spec(specification)
            specification.loader.exec_module(module)
            export = module.TitleExport()
            frame = export.frame(current)
            chapter_card = {"tiles": export.tiles, "palettes": export.palettes, "frame": frame}
            pixels = ppu_render.render_screen(current.bus, top=current.bus.frame_top_last, raster=current.bus.raster_last)
            png.write_rgba(str(output / "reference-chapter-card.png"), 256, 240, ppu_render.to_rgb(pixels))
        current.bus.buttons = 0
        if current.frames == 500:
            current.bus.buttons = 8
        if not chapter_card_seen and last_window and last_window[1] in (0x53, 0x54, 0x57, 0x59, 0x56):
            menu_frames += 1
            if menu_frames % 30 == 0:
                record = last_window[1]
                if record in (0x53, 0x54, 0x59, 0x56):
                    current.bus.buttons = 1
                elif record == 0x57:
                    if create_stage == 0:
                        current.bus.buttons = 1
                    elif create_stage <= 5:
                        current.bus.buttons = 32
                    elif create_stage <= 14:
                        current.bus.buttons = 128
                    elif create_stage == 15:
                        current.bus.buttons = 1
                    create_stage += 1
        if current.frames > 2000 and current.frames % 40 == 0:
            current.bus.buttons = 1
        if current.frames >= 4900:
            current.bus.buttons = 32
        location = (current.bus.ram[0x63], current.bus.ram[0x64])
        if previous_location != location:
            transitions.append({"frame": current.frames, "map": location[0], "submap": location[1],
                                "x": current.bus.ram[0x44], "y": current.bus.ram[0x45],
                                "world_x": current.bus.ram[0x42], "world_y": current.bus.ram[0x43],
                                "interior": bool(current.bus.ram[0x41] & 128)})
            previous_location = location
        if current.bus.buttons:
            inputs.append((current.frames, current.bus.buttons))
        if current.frames in (1200, 1300, 1400, 1500, 1800, 2100, 2250, 2300, 2400, 2600, 2800, 3000, 3200, 3400, 3600, 4800):
            from reference.tools.AssetExtract.dw4assets import ppu_render
            pixels = ppu_render.render_screen(current.bus, top=current.bus.frame_top_last, raster=current.bus.raster_last)
            png.write_rgba(str(output / ("reference-%05d.png" % current.frames)), 256, 240, ppu_render.to_rgb(pixels))
            actor_samples.append({"frame": current.frames,
                "actors": [{"slot": slot, "x": current.bus.read(0x6F60 + slot), "y": current.bus.read(0x6F80 + slot),
                            "direction": current.bus.read(0x7000 + slot) & 3,
                            "shown": bool(current.bus.read(0x70E0 + slot) & 128)} for slot in range(6, 27)]})

    setattr(machine, "on_frame_end", capture)
    machine.run_frames(5600)
    if field_start is None:
        raise ValueError("opening observation did not reach Burland field messages; last window %r" % (last_window,))
    return {"field_start": field_start, "messages": messages, "text_messages": text_messages, "music": music,
            "chapter_card": chapter_card,
            "actor_samples": actor_samples, "inputs": inputs, "transitions": transitions,
            "map": machine.bus.ram[0x63], "submap": machine.bus.ram[0x64],
            "x": machine.bus.ram[0x44], "y": machine.bus.ram[0x45],
            "characters": [machine.bus.read(address) for address in range(0x6001, 0x6001 + 270)],
            "story": [machine.bus.read(address) for address in range(0x627E, 0x62A0)],
            "actors": [{"slot": slot, "x": machine.bus.read(0x6F60 + slot), "y": machine.bus.read(0x6F80 + slot),
                        "direction": machine.bus.read(0x7000 + slot) & 3,
                        "shown": bool(machine.bus.read(0x70E0 + slot) & 128)} for slot in range(6, 27)]}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rom", type=Path, default=ROOT / "reference/build/Release/Dragon Warrior IV (USA).nes")
    parser.add_argument("--output", type=Path, default=ROOT / "native/assets/generated/opening")
    parser.add_argument("--observe", action="store_true")
    parser.add_argument("--verify-curves", action="store_true")
    arguments = parser.parse_args()
    output = arguments.output.resolve()
    if output == ROOT:
        raise ValueError("opening output cannot replace the repository root")
    if output.is_relative_to(ROOT):
        ignored = subprocess.run(["git", "check-ignore", "--quiet", str(output)], cwd=ROOT, timeout=10, check=False)
        if ignored.returncode != 0:
            raise ValueError("opening output is not ignored by Git")
    output.mkdir(parents=True, exist_ok=True)
    rom = Rom(str(arguments.rom))
    if arguments.verify_curves:
        verify_growth(rom)
    document = export_maps(rom, output)
    (output / "index.json").write_text(json.dumps(document, indent=1), encoding="utf-8")
    if arguments.observe:
        observed = observe_opening(rom, output)
        (output / "observed.json").write_text(json.dumps(observed, indent=1), encoding="utf-8")
        print("observed opening:", observed["field_start"], "map", observed["map"], "submap", observed["submap"],
              "transitions", observed["transitions"], "text", observed["text_messages"], "music", observed["music"])
    print("opening exported:", document["tile_count"], "patterns;", len(document["maps"]), "maps")


if __name__ == "__main__":
    main()