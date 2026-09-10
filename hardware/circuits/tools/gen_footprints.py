#!/usr/bin/env python3
"""Generate hardware/circuits/kicad_libs/auto_flight.pretty/: footprints
with no standard-library equivalent.

- Heltec_WiFi_LoRa_32_V3.2: the dev board's 2x18 THT header. Board outline
  and pin pitch are confirmed (50.2 x 25.5mm, 18 pins/row @ 2.54mm, per
  Heltec's own published specs). Row-to-row spacing is NOT confirmed from a
  datasheet drawing (estimated at 20.32mm, a common spacing for this style
  of ESP32 dev board) -- flagged for verification, see tools/README.md,
  before this board is fabricated. Also attaches a user-supplied STEP model
  -- see the comment at its `(model ...)` block below for the rotation
  reasoning, and tools/README.md for how it compares to the other STEP file
  that was considered and not used.

- I2C_Level_Converter: the 4-channel logic-level shifter's 2x6 THT header.
  Pin pitch (2.54mm) and row spacing (11.5mm) both given directly by the
  user (not estimated).
"""
import subprocess
from pathlib import Path

HERE = Path(__file__).parent
OUT_DIR = HERE.parent / "kicad_libs" / "auto_flight.pretty"

PITCH = 2.54


def pad(number, x, y):
    shape = "rect" if number == 1 else "circle"
    return (
        f'\t(pad "{number}" thru_hole {shape}\n'
        f'\t\t(at {x} {y})\n'
        f'\t\t(size 1.7 1.7)\n'
        f'\t\t(drill 1.0)\n'
        f'\t\t(layers "*.Cu" "*.Mask")\n'
        f'\t)\n'
    )


def dual_row_header(name, pins_per_row, row_spacing, descr, tags, board_w=None, board_h=None, model_block=""):
    """A 2-row THT header footprint: pins 1..pins_per_row along the first
    (negative-Y) row left-to-right, pins_per_row+1..2*pins_per_row along the
    second (positive-Y) row left-to-right -- i.e. pin N+1 sits directly
    across from pin N's mirror position, matching how these breakout-style
    parts are silkscreened in practice."""
    x0 = -(pins_per_row - 1) * PITCH / 2
    y0 = -row_spacing / 2
    y1 = row_spacing / 2

    parts = [f'(footprint "{name}"\n']
    parts.append('\t(version 20240108)\n\t(generator "auto_flight_conversion")\n\t(generator_version "1.0")\n')
    parts.append('\t(layer "F.Cu")\n')
    parts.append(f'\t(descr "{descr}")\n')
    parts.append(f'\t(tags "{tags}")\n')
    parts.append(f'\t(property "Reference" "REF**" (at 0 {y0 - 3} 0) (layer "F.SilkS")'
                  f' (effects (font (size 1 1) (thickness 0.15))))\n')
    parts.append(f'\t(property "Value" "{name}" (at 0 {y1 + 3} 0) (layer "F.Fab")'
                  f' (effects (font (size 1 1) (thickness 0.15))))\n')
    parts.append('\t(attr through_hole)\n')

    if board_w and board_h:
        hw, hh = board_w / 2, board_h / 2
        parts.append(f'\t(fp_rect (start {-hw} {-hh}) (end {hw} {hh})\n'
                      f'\t\t(stroke (width 0.15) (type solid)) (fill none) (layer "F.SilkS")\n\t)\n')

    pin = 1
    for i in range(pins_per_row):
        parts.append(pad(pin, x0 + i * PITCH, y0))
        pin += 1
    for i in range(pins_per_row):
        parts.append(pad(pin, x0 + i * PITCH, y1))
        pin += 1

    parts.append(model_block)
    parts.append(')\n')
    return "".join(parts)


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    # A user-supplied STEP model (GrabCAD "Heltec V3 (ESP32 LoRa) - Simplified
    # 3D Model for Mechanical Integration", no embedded color/material data,
    # hence the plain white/gray render). Its native bounding box has the
    # board's long axis on Y (52mm) and short axis on X (25.4mm) -- opposite
    # of this footprint's pin-row-along-X layout -- so it needs a 90 degree Z
    # rotation to align; offset (0,0,0) matched well enough visually that no
    # further nudge was needed (verified via kicad-cli pcb render, not by eye
    # in the KiCad GUI). The earlier auto_flight.3dshapes/*.wrl placeholder
    # box is kept in the repo but no longer referenced.
    heltec_model = (
        '\t(model "${KIPRJMOD}/../kicad_libs/auto_flight.3dshapes/Heltec_WiFi_LoRa_32_V3.2.step"\n'
        '\t\t(offset (xyz 0 0 0))\n'
        '\t\t(scale (xyz 1 1 1))\n'
        '\t\t(rotate (xyz 0 0 90))\n'
        '\t)\n'
    )
    heltec = dual_row_header(
        "Heltec_WiFi_LoRa_32_V3.2", pins_per_row=18, row_spacing=20.32,
        descr="Heltec WiFi LoRa 32 V3.2 dev board, 2x18 THT header, "
              "row spacing ESTIMATED at 20.32mm -- verify before fab, see tools/README.md",
        tags="heltec esp32 lora devboard",
        board_w=50.2, board_h=25.5, model_block=heltec_model,
    )

    level_converter = dual_row_header(
        "I2C_Level_Converter", pins_per_row=6, row_spacing=11.5,
        descr="4-channel bidirectional logic-level converter breakout, 2x6 THT header, "
              "2.54mm pitch, 11.5mm row spacing, 15x13.2mm board outline (all given directly, not estimated)",
        tags="level shifter logic converter i2c",
        # Board is 13.2 x 15mm (given directly): 15mm along the pin rows
        # (comfortably exceeds the 12.7mm span of 6 pins @ 2.54mm) and
        # 13.2mm across them (exactly 11.5mm row spacing + 0.85mm margin
        # each side).
        board_w=15.0, board_h=13.2,
    )

    for name, content in [("Heltec_WiFi_LoRa_32_V3.2", heltec), ("I2C_Level_Converter", level_converter)]:
        out_file = OUT_DIR / f"{name}.kicad_mod"
        out_file.write_text(content)
        print(f"wrote {out_file}")

    tmp_dir = OUT_DIR.parent / "auto_flight_tmp.pretty"
    if tmp_dir.exists():
        for f in tmp_dir.glob("*"):
            f.unlink()
        tmp_dir.rmdir()
    subprocess.run(["kicad-cli", "fp", "upgrade", str(OUT_DIR), "--output", str(tmp_dir)], check=True)
    for f in tmp_dir.glob("*.kicad_mod"):
        f.replace(OUT_DIR / f.name)
    tmp_dir.rmdir()
    print("normalized via kicad-cli fp upgrade")


if __name__ == "__main__":
    main()
