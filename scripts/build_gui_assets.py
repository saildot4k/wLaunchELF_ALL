#!/usr/bin/env python3
import math
import struct
import sys
import zlib
from pathlib import Path

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"

ICON_ORDER = [
    "dir",
    "dir_system",
    "file",
    "file_archive",
    "file_archive_sas",
    "file_executable_ps2",
    "file_executable_windows",
    "file_font",
    "file_game_disc",
    "file_game_disc_compressed",
    "file_game_disc_sheet",
    "file_game_disk",
    "file_game_floppy",
    "file_game_rom",
    "file_game_tape",
    "file_icon_ps2",
    "file_language",
    "file_music",
    "file_patch",
    "file_picture",
    "file_save_other",
    "file_save_ps1",
    "file_save_ps2",
    "file_script",
    "file_system",
    "file_text",
    "file_video",
    "file_video_subtitles",
    "file_vmc_other",
    "file_vmc_ps1",
    "file_vmc_ps2",
]

BUTTON_ORDER = [
    "up",
    "down",
    "left",
    "right",
    "circle",
    "cross",
    "square",
    "triangle",
    "select",
    "start",
    "L1",
    "L2",
    "L3",
    "R1",
    "R2",
    "R3",
    "auto",
]

ASSET_ROOT = Path("gfx/assets")
ICON_ROOT = ASSET_ROOT / "icons"
BUTTON_ROOT = ASSET_ROOT / "buttons"


def paeth(a, b, c):
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def unpack_indices(row, width, bit_depth):
    if bit_depth == 8:
        return list(row[:width])

    mask = (1 << bit_depth) - 1
    indices = []
    for byte in row:
        shift = 8 - bit_depth
        while shift >= 0 and len(indices) < width:
            indices.append((byte >> shift) & mask)
            shift -= bit_depth
        if len(indices) >= width:
            break
    return indices


def gs_alpha(alpha):
    return (alpha * 0x80 + 127) // 255


class PngImage:
    def __init__(self, path):
        self.path = path
        self.width = 0
        self.height = 0
        self.bit_depth = 0
        self.color_type = 0
        self.palette = []
        self.transparency = []
        self.idat = bytearray()
        self.pixels = bytearray()
        self.read()

    def read(self):
        data = self.path.read_bytes()
        if not data.startswith(PNG_SIGNATURE):
            raise ValueError(f"{self.path} is not a PNG")

        pos = len(PNG_SIGNATURE)
        while pos < len(data):
            length = struct.unpack(">I", data[pos:pos + 4])[0]
            pos += 4
            chunk_type = data[pos:pos + 4]
            pos += 4
            chunk = data[pos:pos + length]
            pos += length + 4

            if chunk_type == b"IHDR":
                self.width, self.height, self.bit_depth, self.color_type, compression, png_filter, interlace = struct.unpack(">IIBBBBB", chunk)
                if compression != 0 or png_filter != 0 or interlace != 0:
                    raise ValueError(f"{self.path} uses unsupported PNG encoding")
            elif chunk_type == b"PLTE":
                self.palette = [tuple(chunk[i:i + 3]) for i in range(0, len(chunk), 3)]
            elif chunk_type == b"tRNS":
                self.transparency = list(chunk)
            elif chunk_type == b"IDAT":
                self.idat.extend(chunk)
            elif chunk_type == b"IEND":
                break

        self.pixels = self.decode()

    def bytes_per_pixel_for_filter(self):
        if self.color_type == 0:
            bits = self.bit_depth
        elif self.color_type == 2:
            bits = self.bit_depth * 3
        elif self.color_type == 3:
            bits = self.bit_depth
        elif self.color_type == 6:
            bits = self.bit_depth * 4
        else:
            raise ValueError(f"{self.path} uses unsupported PNG color type {self.color_type}")

        return max(1, math.ceil(bits / 8))

    def row_bytes(self):
        if self.color_type == 0:
            bits = self.width * self.bit_depth
        elif self.color_type == 2:
            bits = self.width * self.bit_depth * 3
        elif self.color_type == 3:
            bits = self.width * self.bit_depth
        elif self.color_type == 6:
            bits = self.width * self.bit_depth * 4
        else:
            raise ValueError(f"{self.path} uses unsupported PNG color type {self.color_type}")

        return math.ceil(bits / 8)

    def decode(self):
        raw = zlib.decompress(bytes(self.idat))
        row_bytes = self.row_bytes()
        bpp = self.bytes_per_pixel_for_filter()
        rows = []
        pos = 0
        prev = bytearray(row_bytes)

        for _ in range(self.height):
            filter_type = raw[pos]
            pos += 1
            row = bytearray(raw[pos:pos + row_bytes])
            pos += row_bytes

            for i in range(row_bytes):
                left = row[i - bpp] if i >= bpp else 0
                up = prev[i]
                up_left = prev[i - bpp] if i >= bpp else 0

                if filter_type == 1:
                    row[i] = (row[i] + left) & 0xff
                elif filter_type == 2:
                    row[i] = (row[i] + up) & 0xff
                elif filter_type == 3:
                    row[i] = (row[i] + ((left + up) >> 1)) & 0xff
                elif filter_type == 4:
                    row[i] = (row[i] + paeth(left, up, up_left)) & 0xff
                elif filter_type != 0:
                    raise ValueError(f"{self.path} uses unsupported PNG filter {filter_type}")

            rows.append(row)
            prev = row

        return self.expand(rows)

    def palette_entry(self, index):
        if index >= len(self.palette):
            raise ValueError(f"{self.path} references palette index {index}, but only has {len(self.palette)} colors")

        red, green, blue = self.palette[index]
        alpha = self.transparency[index] if index < len(self.transparency) else 255
        return red, green, blue, gs_alpha(alpha)

    def expand(self, rows):
        out = bytearray()

        if self.color_type == 3:
            if self.bit_depth not in (1, 2, 4, 8):
                raise ValueError(f"{self.path} uses unsupported indexed bit depth {self.bit_depth}")
            for row in rows:
                for index in unpack_indices(row, self.width, self.bit_depth):
                    out.extend(self.palette_entry(index))
            return out

        if self.color_type == 2:
            if self.bit_depth != 8:
                raise ValueError(f"{self.path} uses unsupported RGB bit depth {self.bit_depth}")
            for row in rows:
                for i in range(0, len(row), 3):
                    out.extend((row[i], row[i + 1], row[i + 2], 0x80))
            return out

        if self.color_type == 6:
            if self.bit_depth != 8:
                raise ValueError(f"{self.path} uses unsupported RGBA bit depth {self.bit_depth}")
            for row in rows:
                for i in range(0, len(row), 4):
                    out.extend((row[i], row[i + 1], row[i + 2], gs_alpha(row[i + 3])))
            return out

        raise ValueError(f"{self.path} uses unsupported PNG color type {self.color_type}")


def load_rgba(path, expected_width, expected_height):
    image = PngImage(path)
    if image.width != expected_width or image.height != expected_height:
        raise ValueError(f"{path} is {image.width}x{image.height}; expected {expected_width}x{expected_height}")
    if image.width % 8 or image.height % 8:
        raise ValueError(f"{path} dimensions must be divisible by 8")
    return image.pixels


def build_icon_atlas():
    icon_width = 32
    icon_height = 16
    columns = 8
    rows = math.ceil(len(ICON_ORDER) / columns)
    atlas_width = columns * icon_width
    atlas_height = rows * icon_height
    atlas = bytearray(atlas_width * atlas_height * 4)

    for index, name in enumerate(ICON_ORDER):
        pixels = load_rgba(ICON_ROOT / f"{name}.png", icon_width, icon_height)
        dst_x = (index % columns) * icon_width
        dst_y = (index // columns) * icon_height

        for y in range(icon_height):
            src_offset = y * icon_width * 4
            dst_offset = ((dst_y + y) * atlas_width + dst_x) * 4
            atlas[dst_offset:dst_offset + icon_width * 4] = pixels[src_offset:src_offset + icon_width * 4]

    return atlas, atlas_width, atlas_height


def build_button_atlas():
    button_height = 32
    button_cell_width = 64
    columns = 4
    rows = math.ceil(len(BUTTON_ORDER) / columns)
    atlas_width = columns * button_cell_width
    atlas_height = rows * button_height
    atlas = bytearray(atlas_width * atlas_height * 4)

    for index, name in enumerate(BUTTON_ORDER):
        button_width = 64 if name == "auto" else 32
        pixels = load_rgba(BUTTON_ROOT / f"{name}.png", button_width, button_height)
        dst_x = (index % columns) * button_cell_width
        dst_y = (index // columns) * button_height

        for y in range(button_height):
            src_offset = y * button_width * 4
            dst_offset = ((dst_y + y) * atlas_width + dst_x) * 4
            atlas[dst_offset:dst_offset + button_width * 4] = pixels[src_offset:src_offset + button_width * 4]

    return atlas, atlas_width, atlas_height


def write_array(out, name, data):
    out.write(f"u8 {name}[] __attribute__((aligned(128))) = {{\n")
    for pos in range(0, len(data), 16):
        values = ", ".join(f"0x{byte:02x}" for byte in data[pos:pos + 16])
        out.write(f"\t{values},\n")
    out.write("};\n\n")


def main():
    output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("src/gui_assets_data.c")
    bg = load_rgba(ASSET_ROOT / "bg.png", 256, 256)
    splash = load_rgba(ASSET_ROOT / "logo_splash.png", 512, 256)
    icons, atlas_width, atlas_height = build_icon_atlas()
    buttons, button_atlas_width, button_atlas_height = build_button_atlas()

    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", newline="\n") as out:
        out.write("/* Generated by scripts/build_gui_assets.py. */\n")
        out.write("#include <tamtypes.h>\n\n")
        out.write(f"const unsigned int gui_asset_icon_atlas_width = {atlas_width};\n")
        out.write(f"const unsigned int gui_asset_icon_atlas_height = {atlas_height};\n\n")
        out.write(f"const unsigned int gui_asset_button_atlas_width = {button_atlas_width};\n")
        out.write(f"const unsigned int gui_asset_button_atlas_height = {button_atlas_height};\n\n")
        write_array(out, "gui_asset_bg_rgba", bg)
        write_array(out, "gui_asset_splash_rgba", splash)
        write_array(out, "gui_asset_icons_rgba", icons)
        write_array(out, "gui_asset_buttons_rgba", buttons)


if __name__ == "__main__":
    main()
