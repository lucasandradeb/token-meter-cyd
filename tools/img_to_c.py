#!/usr/bin/env python3
"""Converte uma imagem (PNG/WEBP/JPG/GIF-1o-frame) em uma imagem LVGL para o
topo da placa. Redimensiona pra caber no espaco (~300x62) sem PSRAM.

Uso:
    python tools/img_to_c.py CAMINHO_DA_IMAGEM [--maxw 300] [--maxh 62]

Gera firmware/src/app/party_img_user.c/.h (gitignored). Depois e so regravar:
o ui.cpp detecta o arquivo local automaticamente e usa no lugar da arte padrao.

IMPORTANTE (direitos autorais): use apenas imagens que voce tem o direito de
usar. O arquivo gerado NAO entra no repositorio (gitignored). Nao distribua
personagens/arte de terceiros num repo publico.
"""
from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image

OUT = Path("firmware/src/app/party_img_user.c")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("img")
    ap.add_argument("--maxw", type=int, default=300)
    ap.add_argument("--maxh", type=int, default=62)
    a = ap.parse_args()

    im = Image.open(a.img).convert("RGBA")
    im.thumbnail((a.maxw, a.maxh), Image.LANCZOS)  # mantem proporcao
    w, h = im.size
    px = im.load()

    data = bytearray()
    for y in range(h):
        for x in range(w):
            r, g, b, al = px[x, y]
            data += bytes((b, g, r, al))  # LVGL ARGB8888 = B,G,R,A

    ram = w * h * 4
    lines = ["// GERADO por tools/img_to_c.py — NAO commitar (gitignored).",
             '#include "lvgl.h"', "",
             f"// {w}x{h}, {len(data)} bytes de flash.",
             "static const uint8_t party_img_user_data[] = {"]
    hexs = ["0x%02x" % v for v in data]
    for i in range(0, len(hexs), 20):
        lines.append("    " + ",".join(hexs[i:i + 20]) + ",")
    lines += ["};", "",
              "const lv_image_dsc_t party_img_user_dsc = {",
              "    .header = { .magic = LV_IMAGE_HEADER_MAGIC,",
              "                .cf = LV_COLOR_FORMAT_ARGB8888,",
              f"               .w = {w}, .h = {h} }},",
              "    .data_size = sizeof(party_img_user_data),",
              "    .data = party_img_user_data,",
              "};"]
    OUT.write_text("\n".join(lines) + "\n")
    OUT.with_suffix(".h").write_text(
        '#include "lvgl.h"\nextern const lv_image_dsc_t party_img_user_dsc;\n')
    print(f"OK -> {OUT.name} e .h ({w}x{h}, {len(data)} bytes flash, "
          f"~{ram} bytes usados no desenho). Agora e so regravar.")


if __name__ == "__main__":
    main()
