from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from reference.tools.AssetExtract.dw4assets import nes, png, ppu_render
from reference.tools.AssetExtract.dw4assets.nesbus import Machine
from reference.tools.AssetExtract.dw4assets.rom import Rom


class TitleExport:
    def __init__(self) -> None:
        self.tiles: list[list[int]] = []
        self.tile_ids: dict[bytes, int] = {}
        self.palettes: list[list[list[int]]] = []
        self.palette_ids: dict[tuple[int, ...], int] = {}

    def tile(self, data: bytes) -> int:
        if data not in self.tile_ids:
            if len(self.tiles) >= 8192:
                raise ValueError("title exceeded the 8192-pattern limit")
            self.tile_ids[data] = len(self.tiles)
            self.tiles.append(nes.decode_tile_2bpp(data))
        return self.tile_ids[data]

    def palette(self, values: list[int]) -> int:
        key = tuple(values)
        if key not in self.palette_ids:
            if len(self.palettes) >= 256:
                raise ValueError("title exceeded the palette limit")
            self.palette_ids[key] = len(self.palettes)
            self.palettes.append([list(nes.rgb(value)) + [0 if index == 0 else 255]
                                  for index, value in enumerate(values)])
        return self.palette_ids[key]

    def frame(self, machine: Machine) -> dict:
        bus = machine.bus
        bus.oam[:] = bytes(bus.ram[0x200:0x300])
        colors = ppu_render.palette_ram(bus.vram)
        background = list(nes.rgb(colors[0])) + [255]
        if not bus.ppumask & 0x18:
            return {"background": background, "draws": []}
        scrolls = ppu_render.line_scrolls(bus, bus.frame_top_last, bus.raster_last)
        bands: list[tuple[int, int, tuple[int, int, int]]] = []
        first = 0
        for line in range(1, 241):
            if line == 240 or scrolls[line] != scrolls[first]:
                bands.append((first, line - first, scrolls[first]))
                first = line
        draws: list[list[int]] = []
        background_table = 0x1000 if bus.ppuctrl & 0x10 else 0
        for top, height, (scroll_x, select, scroll_y) in bands:
            base_x = (select & 1) * 256 + scroll_x
            base_y = ((select >> 1) & 1) * 240 + scroll_y
            for row in range((base_y + top) // 8, (base_y + top + height - 1) // 8 + 1):
                world_y = (row * 8) % 480
                table_y, local_y = divmod(world_y, 240)
                for column in range(base_x // 8, (base_x + 255) // 8 + 1):
                    world_x = (column * 8) % 512
                    table_x, local_x = divmod(world_x, 256)
                    table = 0x2000 + 0x400 * (table_x + 2 * table_y)
                    number = bus.vram[bus._vram_index(table + (local_y // 8) * 32 + local_x // 8)]
                    data = bytes(bus.vram[background_table + number * 16:background_table + number * 16 + 16])
                    if not any(data):
                        continue
                    attribute = bus.vram[bus._vram_index(table + 0x3C0 + (local_y // 32) * 8 + local_x // 32)]
                    shift = ((local_y // 16) & 1) * 4 + ((local_x // 16) & 1) * 2
                    palette = (attribute >> shift) & 3
                    palette_id = self.palette(colors[palette * 4:palette * 4 + 4])
                    draws.append([self.tile(data), column * 8 - base_x, row * 8 - base_y,
                                  palette_id, 0, top, height])
        sprite_table = 0x1000 if bus.ppuctrl & 8 else 0
        if bus.ppuctrl & 0x20:
            raise ValueError("title export encountered unsupported tall sprites")
        for number in range(63, -1, -1):
            vertical, tile, flags, horizontal = bus.oam[number * 4:number * 4 + 4]
            if vertical >= 0xEF:
                continue
            data = bytes(bus.vram[sprite_table + tile * 16:sprite_table + tile * 16 + 16])
            if not any(data):
                continue
            palette_id = self.palette(colors[0x10 + (flags & 3) * 4:0x14 + (flags & 3) * 4])
            draw = [self.tile(data), horizontal, vertical + 1, palette_id,
                    (1 if flags & 0x40 else 0) | (2 if flags & 0x80 else 0), 0, 240]
            if flags & 0x20:
                draws.insert(0, draw)
            else:
                draws.append(draw)
        if len(draws) > 4096:
            raise ValueError("title frame exceeded its draw limit")
        return {"background": background, "draws": draws}


def extract(rom_path: Path, output: Path) -> None:
    machine = Machine(Rom(str(rom_path)))
    machine.bus.chr0 = 0x10
    machine.cpu.reset()
    export = TitleExport()
    frames: list[dict] = []
    artwork_frame: int | None = None
    fade_start: int | None = None
    fade_end: int | None = None
    music: list[dict[str, int]] = []
    press_frame = 3100
    capture_frames = 3140
    comparison_frames = (31, 183, 384, 673, 1556)
    if not output.is_dir():
        output.mkdir(parents=True)

    def observe_artwork(_cpu) -> None:
        nonlocal artwork_frame
        if machine.bus.bank_at(machine.cpu.pc) == 0x1B and artwork_frame is None:
            artwork_frame = machine.frames

    def observe_fade(_cpu) -> None:
        nonlocal fade_start
        if machine.bus.bank_at(machine.cpu.pc) == 0x1B and fade_start is None:
            fade_start = machine.frames

    def observe_finish(_cpu) -> None:
        nonlocal fade_end
        if machine.bus.bank_at(machine.cpu.pc) == 0x1B and fade_start is not None and fade_end is None:
            fade_end = machine.frames

    def observe_music(cpu) -> None:
        if machine.bus.bank_at(cpu.pc) == 0x1F and 0 < cpu.a < 0x80:
            music.append({"frame": machine.frames, "track": cpu.a})

    machine.cpu.hooks[0xA7ED] = observe_artwork
    machine.cpu.hooks[0xA809] = observe_fade
    machine.cpu.hooks[0xA80C] = observe_finish
    machine.cpu.hooks[0xEF5B] = observe_music

    def capture(current: Machine) -> None:
        frames.append(export.frame(current))
        if current.frames in comparison_frames:
            colors = ppu_render.render_screen(current.bus, top=current.bus.frame_top_last, raster=current.bus.raster_last)
            png.write_rgba(str(output / ("reference-%05d.png" % current.frames)), 256, 240, ppu_render.to_rgb(colors))
        current.bus.buttons = 8 if current.frames == press_frame else 0

    setattr(machine, "on_frame_end", capture)
    machine.run_frames(capture_frames)
    if artwork_frame is None or fade_start is None or fade_end is None or not 1 <= fade_end - fade_start <= 120:
        raise ValueError("title control boundaries were not observed")
    loop_start = 1560
    loop_frames = 1024
    for index in range(loop_start, press_frame - loop_frames):
        if frames[index] != frames[index + loop_frames]:
            raise ValueError("title scroll period did not repeat at frame %d" % (index + 1))
    width, height, pixels = nes.tile_sheet(export.tiles, 16)
    png.write_indexed(str(output / "patterns.png"), width, height, pixels, nes.GRAY4)
    draw_records: list[list[int]] = []
    draw_ids: dict[tuple[int, ...], int] = {}
    for frame in frames:
        indexes = []
        for draw in frame["draws"]:
            key = tuple(draw)
            if key not in draw_ids:
                draw_ids[key] = len(draw_records)
                draw_records.append(draw)
            indexes.append(draw_ids[key])
        frame["draws"] = indexes
    document = {
        "schema_version": 1,
        "image": "patterns.png",
        "tiles": len(export.tiles),
        "artwork_frame": artwork_frame,
        "fade_frames": fade_end - fade_start,
        "loop_start": loop_start,
        "loop_frames": loop_frames,
        "music": [event for event in music if event["frame"] < press_frame],
        "palettes": export.palettes,
        "draw_records": draw_records,
        "frames": frames[:press_frame],
        "exit_frames": frames[fade_start:fade_end],
        "evidence": {"entry": "1B:A4EF", "artwork_loop": "1B:A7ED", "fade": "1B:A809-A80C",
                     "source_press_frame": press_frame, "observed_fade_start": fade_start, "observed_fade_end": fade_end},
    }
    payload = json.dumps(document, separators=(",", ":"))
    if len(payload) > 64 * 1024 * 1024:
        raise ValueError("title document exceeds 64 MiB")
    (output / "index.json").write_text(payload, encoding="utf-8")
    print("title exported:", len(export.tiles), "patterns,", len(export.palettes), "palettes; artwork frame", artwork_frame,
          "fade", fade_end - fade_start, "frames")
    print("title music:", document["music"])


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    arguments = parser.parse_args()
    if arguments.out.resolve() == ROOT:
        parser.error("title output cannot be the repository root")
    subprocess.run(["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                    str(ROOT / "scripts" / "port" / "assert-asset-output-ignored.ps1"),
                    "-RepositoryRoot", str(ROOT), "-OutputPath", str(arguments.out.resolve())], check=True, timeout=15)
    extract(arguments.rom, arguments.out)


if __name__ == "__main__":
    main()