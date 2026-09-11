#!/usr/bin/env python3
"""Generate a .kicad_sch from <board>.parts.json / <board>.nets.json.

Symbols are placed on a plain grid (not Fritzing's coordinates -- those
don't translate to sensible KiCad spacing, see tools/README.md) and wired
by same-name local labels dropped directly on each pin's connection point
(no routed wire geometry). One power:PWR_FLAG per multi-member net keeps
ERC from flagging power-type pins as undriven.
"""
import argparse
import json
import subprocess
import uuid
from pathlib import Path

from parts_catalog import CATALOG
from sexpr_utils import extract_symbol_block, rename_symbol_block, extract_pins

HERE = Path(__file__).parent
STD_LIB_DIR = Path("/Applications/KiCad/KiCad.app/Contents/SharedSupport/symbols")
CUSTOM_SYM_FILE = HERE.parent / "kicad_libs" / "auto_flight.kicad_sym"

# Net names cleaned up by hand after the Phase-0 audit: fz_extract.py's
# majority-vote naming still falls back to a synthesized name when a net's
# member pin descriptions are too varied (e.g. the plane's main ground net
# includes a transistor pin literally described "C", diluting the "GND"
# majority below 50%). See tools/README.md.
NET_RENAME = {
    # Renamed to GND_MAIN, not GND: a plain "GND" already exists as its own
    # distinct (singleton, unused) net -- Heltec's second GND pin, which the
    # Fritzing source never actually ties to this one. Renaming to the same
    # name would silently merge them (dict key collision) and fabricate a
    # connection the original design doesn't have.
    "NET_13879_connector0_MULTI": "GND_MAIN",
    "NET_16098_connector3_MULTI": "3V3",
    "NET_16098_connector8_MULTI": "5V",
    "NET_16098_connector6_MULTI": "SDA_5V",
    "NET_16098_connector7_MULTI": "SCL_5V",

    # The rest of the plane's remaining "NET_..._MULTI" placeholders, named
    # by function instead of by connectorId (SDA/SCL/GND/3V3/5V above were
    # already the model for this — these are just the ones majority-vote
    # naming couldn't resolve on its own, mostly because a resistor's or
    # transistor's own pin description ["Pin 0"/"B"/"E"] never matches the
    # net's real function).
    "NET_13879_connector1_MULTI": "SBUS_BASE",       # R2 <-> transistor base (U1.B)
    "NET_13879_connector2_MULTI": "UART_RX",         # transistor emitter (U1.E) -> Arduino D0/RX
    "NET_17653_connector2_MULTI": "GPS_TX",          # GPS module TX -> Heltec GPIO5
    "NET_34929_connector2_MULTI": "SBUS_SIGNAL",     # SBUS receiver pulse -> R2 (pre-inversion)
    "NET_39454_connector2_MULTI": "SERVO1_SIGNAL",   # J1 -> Arduino D9
    "NET_39457_connector2_MULTI": "SERVO2_SIGNAL",   # J2 -> Arduino D10
    "NET_39460_connector2_MULTI": "SERVO3_SIGNAL",   # J3 -> Arduino D11/MOSI
    "NET_39463_connector2_MULTI": "SERVO4_SIGNAL",   # J4 -> Arduino D12/MISO
    "NET_41941_connector14_MULTI": "VBAT_A0",        # ADS1115 A0 <-> Battery Balancer pin 3
    "NET_41941_connector15_MULTI": "VBAT_A1",        # ADS1115 A1 <-> Battery Balancer pin 2
    "NET_41941_connector16_MULTI": "VBAT_A2",        # ADS1115 A2 <-> Battery Balancer pin 1
}


def uid():
    return str(uuid.uuid4())


def get_symbol_block(lib_id: str, custom_text: str) -> str:
    libname, symname = lib_id.split(":", 1)
    if libname == "auto_flight":
        block = extract_symbol_block(custom_text, symname)
    else:
        text = (STD_LIB_DIR / f"{libname}.kicad_sym").read_text()
        block = extract_symbol_block(text, symname)
    return rename_symbol_block(block, symname, lib_id)


def build(board_dir: Path, stem: str, project_name: str):
    parts = json.loads((board_dir / f"{stem}.parts.json").read_text())
    nets = json.loads((board_dir / f"{stem}.nets.json").read_text())
    renamed = {}
    for k, v in nets.items():
        new_k = NET_RENAME.get(k, k)
        if new_k in renamed:
            raise ValueError(f"NET_RENAME collision: both an existing net and "
                              f"{k!r} map to {new_k!r} -- would silently merge two distinct nets")
        renamed[new_k] = v
    nets = renamed
    generated_pin_numbers = json.loads((HERE / "generated_pin_numbers.json").read_text())
    custom_text = CUSTOM_SYM_FILE.read_text()

    real_parts = sorted((p for p in parts if p["kind"] == "part"), key=lambda p: p["title"])
    module_id_refs = {p["module_id_ref"] for p in real_parts}

    lib_blocks = {}
    for mid in module_id_refs:
        lib_id = CATALOG[mid]["symbol"]
        lib_blocks.setdefault(lib_id, get_symbol_block(lib_id, custom_text))
    lib_blocks["power:PWR_FLAG"] = get_symbol_block("power:PWR_FLAG", custom_text)

    pin_geom = {lib_id: extract_pins(block) for lib_id, block in lib_blocks.items()}

    n_cols = max(1, round(len(real_parts) ** 0.5))
    # Multiples of 2.54mm so every pin (spaced in 1.27/2.54mm steps in both
    # our generated and KiCad's standard symbols) lands on the schematic's
    # native connection grid -- avoids spurious ERC "off grid" warnings.
    cell_w, cell_h = 60.96, 76.2
    origin_x, origin_y = 50.8, 50.8

    sch_uuid = uid()
    placements = {}
    symbol_chunks = []

    for i, part in enumerate(real_parts):
        spec = CATALOG[part["module_id_ref"]]
        lib_id = spec["symbol"]
        col, row = i % n_cols, i // n_cols
        sx = round(origin_x + col * cell_w, 2)
        sy = round(origin_y + row * cell_h, 2)
        ref = part["title"].replace(" ", "_")
        footprint = spec.get("footprint") or ""

        pin_map = {}
        if spec["kind"] == "generated":
            pin_map = generated_pin_numbers[part["module_id_ref"]]
        else:
            fn = spec["pin_number"]
            for p in part["pins"]:
                pin_map[p["connector_id"]] = fn(p["name"])

        geom = pin_geom[lib_id]
        pins_abs = {}
        for connector_id, number in pin_map.items():
            if number not in geom:
                raise KeyError(f"{part['title']}: pin {number} (connector {connector_id}) "
                                f"not found in {lib_id} geometry {sorted(geom)}")
            lx, ly, _ = geom[number]
            # Symbol-library space has +Y up; schematic-sheet space has +Y
            # down -- flip the local Y offset when placing at rotation 0.
            pins_abs[connector_id] = (number, round(sx + lx, 3), round(sy - ly, 3))
        placements[part["model_index"]] = pins_abs

        pin_uuid_lines = "".join(f'\t\t(pin "{n}" (uuid "{uid()}"))\n' for n in geom)
        sym_uuid = uid()
        symbol_chunks.append(
            f'\t(symbol\n\t\t(lib_id "{lib_id}")\n\t\t(at {sx} {sy} 0)\n\t\t(unit 1)\n'
            f'\t\t(exclude_from_sim no)\n\t\t(in_bom yes)\n\t\t(on_board yes)\n\t\t(dnp no)\n'
            f'\t\t(uuid "{sym_uuid}")\n'
            f'\t\t(property "Reference" "{ref}" (at {sx} {sy - 6} 0) (effects (font (size 1.27 1.27))))\n'
            f'\t\t(property "Value" "{part["title"]}" (at {sx} {sy - 4} 0) (effects (font (size 1.27 1.27))))\n'
            f'\t\t(property "Footprint" "{footprint}" (at {sx} {sy} 0) (effects (font (size 1.27 1.27)) hide))\n'
            f'{pin_uuid_lines}'
            f'\t\t(instances (project "{project_name}" (path "/{sch_uuid}" (reference "{ref}") (unit 1))))\n'
            f'\t)\n'
        )

    other_chunks = []
    for net_name, members in nets.items():
        placed_members = []
        for m in members:
            entry = placements.get(m["model_index"], {}).get(m["connector_id"])
            if entry:
                placed_members.append(entry)
        if len(placed_members) < 2:
            continue
        anchor = None
        for number, ax, ay in placed_members:
            if anchor is None:
                anchor = (ax, ay)
            other_chunks.append(
                f'\t(label "{net_name}" (at {ax} {ay} 0) '
                f'(effects (font (size 1.27 1.27)) (justify left bottom)) (uuid "{uid()}"))\n'
            )
        fx, fy = anchor
        flag_ref = f"#FLG{uid()[:8]}"
        other_chunks.append(
            f'\t(symbol (lib_id "power:PWR_FLAG") (at {fx} {fy} 0) (unit 1) '
            f'(exclude_from_sim no) (in_bom yes) (on_board yes) (dnp no) (uuid "{uid()}")\n'
            f'\t\t(property "Reference" "{flag_ref}" (at {fx} {fy} 0) (effects (font (size 1.27 1.27)) hide))\n'
            f'\t\t(property "Value" "PWR_FLAG" (at {fx} {fy} 0) (effects (font (size 1.27 1.27)) hide))\n'
            f'\t\t(pin "1" (uuid "{uid()}"))\n'
            f'\t\t(instances (project "{project_name}" (path "/{sch_uuid}" (reference "{flag_ref}") (unit 1))))\n'
            f'\t)\n'
        )

    lib_symbols_block = "\t(lib_symbols\n" + "\n".join(lib_blocks.values()) + "\n\t)\n"

    n_rows = (len(real_parts) + n_cols - 1) // n_cols
    paper = "A2" if max(n_cols, n_rows) > 4 else "A3"

    out = [
        "(kicad_sch\n\t(version 20250114)\n\t(generator \"auto_flight_conversion\")\n"
        "\t(generator_version \"1.0\")\n",
        f'\t(uuid "{sch_uuid}")\n',
        f'\t(paper "{paper}")\n',
        lib_symbols_block,
        "".join(symbol_chunks),
        "".join(other_chunks),
        '\t(sheet_instances\n\t\t(path "/" (page "1"))\n\t)\n',
        ")\n",
    ]
    return "".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("board_dir", type=Path)
    ap.add_argument("stem")
    ap.add_argument("project_name")
    args = ap.parse_args()

    text = build(args.board_dir, args.stem, args.project_name)
    out_file = args.board_dir / f"{args.project_name}.kicad_sch"
    out_file.write_text(text)
    print(f"wrote {out_file}")

    subprocess.run(["kicad-cli", "sch", "upgrade", "--force", str(out_file)], check=True)
    print("normalized via kicad-cli sch upgrade")


if __name__ == "__main__":
    main()
