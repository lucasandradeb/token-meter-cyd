#!/usr/bin/env python3
"""Converte um GIF em um arquivo C para o mascote animado da placa (LVGL lv_gif).

A placa NAO tem PSRAM, entao o GIF precisa ser pequeno: o decoder do LVGL aloca
um buffer de frame de W*H*4 bytes. Use ~72x72 (padrao) ou menor.

Uso:
    python tools/gif_to_c.py CAMINHO_DO_GIF [--size 72] [--frames 24] \
        [--out firmware/src/app/mascot_gif.c]

Depois compile com  -DUSE_GIF_MASCOT  (adicione em build_flags do env app).

IMPORTANTE (direitos autorais): use apenas um GIF que voce tem o direito de
usar. Nao inclua no repositorio publico assets protegidos de terceiros. O
arquivo gerado (mascot_gif.c) e gitignored justamente por isso.
"""
from __future__ import annotations

import argparse
import io
from pathlib import Path

from PIL import Image

BG = (0x0D, 0x0F, 0x0C)  # cor de fundo da UI (mascote sem transparencia)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("gif")
    ap.add_argument("--size", type=int, default=72, help="lado (px), quadrado")
    ap.add_argument("--frames", type=int, default=24, help="max de frames")
    ap.add_argument("--out", default="firmware/src/app/mascot_gif.c")
    a = ap.parse_args()

    im = Image.open(a.gif)
    total = getattr(im, "n_frames", 1)
    step = max(1, total // a.frames)
    idxs = list(range(0, total, step))[: a.frames]

    frames = []
    for i in idxs:
        im.seek(i)
        fr = im.convert("RGBA").resize((a.size, a.size), Image.NEAREST)
        bg = Image.new("RGBA", fr.size, BG + (255,))
        bg.alpha_composite(fr)
        frames.append(bg.convert("RGB"))

    buf = io.BytesIO()
    frames[0].save(buf, format="GIF", save_all=True, append_images=frames[1:],
                   loop=0, duration=im.info.get("duration", 80), optimize=True)
    data = buf.getvalue()

    ram = a.size * a.size * 4
    out = Path(a.out)
    lines = [
        "// GERADO por tools/gif_to_c.py — NAO commitar (gitignored).",
        '#include "lvgl.h"', "",
        f"// {a.size}x{a.size}, {len(frames)} frames, {len(data)} bytes.",
        f"// RAM de decode ~= {ram} bytes ({a.size}*{a.size}*4).",
        "static const uint8_t mascot_gif_data[] = {",
    ]
    hexs = ["0x%02x" % b for b in data]
    for i in range(0, len(hexs), 16):
        lines.append("    " + ",".join(hexs[i:i + 16]) + ",")
    lines += [
        "};", "",
        "const lv_image_dsc_t mascot_gif_dsc = {",
        "    .header = { .magic = LV_IMAGE_HEADER_MAGIC,",
        "                .cf = LV_COLOR_FORMAT_RAW,",
        f"               .w = {a.size}, .h = {a.size} }},",
        "    .data_size = sizeof(mascot_gif_data),",
        "    .data = mascot_gif_data,",
        "};",
    ]
    out.write_text("\n".join(lines) + "\n")
    print(f"OK -> {out} ({a.size}x{a.size}, {len(frames)} frames, "
          f"{len(data)} bytes de flash, ~{ram} bytes de RAM de decode).")
    print("Agora compile o env app com -DUSE_GIF_MASCOT.")


if __name__ == "__main__":
    main()
