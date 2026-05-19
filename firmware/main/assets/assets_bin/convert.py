#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Convert StackChan launcher UI assets between PNG and LVGL v9 binary (.bin).

Binary layout: 12-byte lv_image_header_t + pixel payload.
Supported formats: RGB565 (0x12), RGB565A8 (0x14, color plane then alpha plane).

Requires: pip install pillow
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError as exc:
    raise SystemExit("Pillow is required: pip install pillow") from exc

LV_IMAGE_HEADER_SIZE = 12
LV_IMAGE_HEADER_MAGIC = 0x19
LV_COLOR_FORMAT_RGB565 = 0x12
LV_COLOR_FORMAT_RGB565A8 = 0x14

CF_NAMES = {
    LV_COLOR_FORMAT_RGB565: "RGB565",
    LV_COLOR_FORMAT_RGB565A8: "RGB565A8",
}


def _pack_lvgl_v9_header(w: int, h: int, cf: int) -> bytes:
    magic = LV_IMAGE_HEADER_MAGIC
    flags = 0
    stride = w * 2
    reserved2 = 0
    hdr = struct.pack("<I", magic | (cf << 8) | (flags << 16))
    hdr += struct.pack("<I", w | (h << 16))
    hdr += struct.pack("<I", stride | (reserved2 << 16))
    return hdr


def _parse_lvgl_v9_header(data: bytes) -> tuple[int, int, int, int, int]:
    if len(data) < LV_IMAGE_HEADER_SIZE:
        raise ValueError(f"expected at least {LV_IMAGE_HEADER_SIZE} header bytes, got {len(data)}")

    w0, w1, w2 = struct.unpack("<III", data[:LV_IMAGE_HEADER_SIZE])
    magic = w0 & 0xFF
    cf = (w0 >> 8) & 0xFF
    w = w1 & 0xFFFF
    h = (w1 >> 16) & 0xFFFF
    stride = w2 & 0xFFFF

    if magic != LV_IMAGE_HEADER_MAGIC:
        raise ValueError(f"invalid LVGL image magic 0x{magic:02x} (expected 0x{LV_IMAGE_HEADER_MAGIC:02x})")
    if cf not in CF_NAMES:
        supported = ", ".join(f"0x{k:02x} ({v})" for k, v in CF_NAMES.items())
        raise ValueError(f"unsupported color format 0x{cf:02x} (supported: {supported})")
    if w <= 0 or h <= 0:
        raise ValueError(f"invalid image size {w}x{h}")
    if stride < w * 2:
        raise ValueError(f"stride {stride} too small for width {w} RGB565")

    return magic, cf, w, h, stride


def _pixel_payload_size(cf: int, w: int, h: int, stride: int) -> int:
    color_bytes = stride * h
    if cf == LV_COLOR_FORMAT_RGB565:
        return color_bytes
    if cf == LV_COLOR_FORMAT_RGB565A8:
        return color_bytes + (stride // 2) * h
    raise ValueError(f"unsupported color format 0x{cf:02x}")


def _rgb888_to_rgb565(r: int, g: int, b: int) -> int:
    r5 = (r * 31) // 255
    g6 = (g * 63) // 255
    b5 = (b * 31) // 255
    return (r5 << 11) | (g6 << 5) | b5


def _rgb565_to_rgb888(word: int) -> tuple[int, int, int]:
    r5 = (word >> 11) & 0x1F
    g6 = (word >> 5) & 0x3F
    b5 = word & 0x1F
    r = (r5 * 255) // 31
    g = (g6 * 255) // 63
    b = (b5 * 255) // 31
    return r, g, b


def _png_has_alpha(img: Image.Image) -> bool:
    if img.mode != "RGBA":
        return False
    lo, hi = img.getextrema()[3]
    return lo < 255


def png_to_bin(png_path: Path) -> bytes:
    img = Image.open(png_path).convert("RGBA")
    w, h = img.size
    use_alpha = _png_has_alpha(img)

    color_plane = bytearray()
    alpha_plane = bytearray()
    for y in range(h):
        for x in range(w):
            r, g, b, a = img.getpixel((x, y))
            if not use_alpha and a < 255:
                inv = 255 - a
                r = (r * a + 255 * inv) // 255
                g = (g * a + 255 * inv) // 255
                b = (b * a + 255 * inv) // 255
            word = _rgb888_to_rgb565(r, g, b)
            color_plane.extend(struct.pack("<H", word))
        if use_alpha:
            for x in range(w):
                alpha_plane.append(img.getpixel((x, y))[3])

    if use_alpha:
        cf = LV_COLOR_FORMAT_RGB565A8
        payload = bytes(color_plane) + bytes(alpha_plane)
        expected = _pixel_payload_size(cf, w, h, w * 2)
    else:
        cf = LV_COLOR_FORMAT_RGB565
        payload = bytes(color_plane)
        expected = w * h * 2

    if len(payload) != expected:
        raise ValueError(f"internal error: pixel buffer {len(payload)} != {expected}")

    return _pack_lvgl_v9_header(w, h, cf) + payload


def bin_to_png(bin_path: Path) -> Image.Image:
    data = bin_path.read_bytes()
    _, cf, w, h, stride = _parse_lvgl_v9_header(data)

    payload_size = _pixel_payload_size(cf, w, h, stride)
    pixel_data = data[LV_IMAGE_HEADER_SIZE:]
    if len(pixel_data) < payload_size:
        raise ValueError(f"truncated pixel data: need {payload_size} bytes, got {len(pixel_data)}")

    color_bytes = stride * h
    color_plane = pixel_data[:color_bytes]
    alpha_plane = pixel_data[color_bytes:payload_size] if cf == LV_COLOR_FORMAT_RGB565A8 else None
    alpha_stride = stride // 2

    img = Image.new("RGBA", (w, h))
    for y in range(h):
        row_base = y * stride
        a_row_base = y * alpha_stride
        for x in range(w):
            word = struct.unpack_from("<H", color_plane, row_base + x * 2)[0]
            r, g, b = _rgb565_to_rgb888(word)
            if alpha_plane is not None:
                a = alpha_plane[a_row_base + x]
            else:
                a = 255
            img.putpixel((x, y), (r, g, b, a))

    return img


def _infer_direction(input_path: Path) -> str:
    ext = input_path.suffix.lower()
    if ext == ".png":
        return "bin"
    if ext == ".bin":
        return "png"
    raise ValueError(f"cannot infer conversion from extension {ext!r}; use --to png or --to bin")


def _resolve_output(input_path: Path, to_ext: str, output: Path | None) -> Path:
    if output is not None:
        return output.resolve()
    return input_path.with_suffix(f".{to_ext}").resolve()


def convert(input_path: Path, to: str, output: Path | None) -> Path:
    input_path = input_path.resolve()
    if not input_path.is_file():
        raise FileNotFoundError(input_path)

    to_ext = to.lstrip(".").lower()
    if to_ext not in ("png", "bin"):
        raise ValueError(f"unsupported target {to!r}")

    out_path = _resolve_output(input_path, to_ext, output)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    if to_ext == "bin":
        if input_path.suffix.lower() != ".png":
            raise ValueError("PNG input required for --to bin")
        blob = png_to_bin(input_path)
        out_path.write_bytes(blob)
    else:
        if input_path.suffix.lower() != ".bin":
            raise ValueError("LVGL .bin input required for --to png")
        img = bin_to_png(input_path)
        img.save(out_path, format="PNG")

    return out_path


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert launcher UI assets between PNG and LVGL v9 .bin (RGB565 / RGB565A8).",
    )
    parser.add_argument(
        "input",
        type=Path,
        help="Source file (.png or .bin)",
    )
    parser.add_argument(
        "--to",
        choices=("png", "bin"),
        dest="to_format",
        help="Output format (default: .png -> bin, .bin -> png)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="Output file path (default: same name, other extension)",
    )
    args = parser.parse_args()

    try:
        to = args.to_format or _infer_direction(args.input)
        out = convert(args.input, to, args.output)
    except (OSError, ValueError, FileNotFoundError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(f"Wrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
