#!/usr/bin/env python3
"""Generate a .kicad_pcb from <board>.parts.json / <board>.nets.json:
board outline, footprints placed on a simple grid (not Fritzing's pcbView
coordinates -- their unit scale couldn't be confirmed, see tools/README.md),
and pads wired to the same nets used in the schematic. No copper routing is
generated; DRC is expected to show every net as an unrouted ratsnest.
"""
import argparse
import json
import re
import subprocess
import uuid
from pathlib import Path

from parts_catalog import CATALOG
from sexpr_utils import find_balanced

HERE = Path(__file__).parent
STD_FP_DIR = Path("/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints")
CUSTOM_FP_DIR = HERE.parent / "kicad_libs" / "auto_flight.pretty"

HEADER = '''(kicad_pcb
\t(version 20250114)
\t(generator "auto_flight_conversion")
\t(generator_version "1.0")
\t(general
\t\t(thickness 1.6)
\t)
\t(paper "A3")
\t(layers
\t\t(0 "F.Cu" signal)
\t\t(2 "B.Cu" signal)
\t\t(9 "F.Adhes" user "F.Adhesive")
\t\t(11 "B.Adhes" user "B.Adhesive")
\t\t(13 "F.Paste" user)
\t\t(15 "B.Paste" user)
\t\t(5 "F.SilkS" user "F.Silkscreen")
\t\t(7 "B.SilkS" user "B.Silkscreen")
\t\t(1 "F.Mask" user)
\t\t(3 "B.Mask" user)
\t\t(17 "Dwgs.User" user "User.Drawings")
\t\t(19 "Cmts.User" user "User.Comments")
\t\t(21 "Eco1.User" user "User.Eco1")
\t\t(23 "Eco2.User" user "User.Eco2")
\t\t(25 "Edge.Cuts" user)
\t\t(27 "Margin" user)
\t\t(31 "F.CrtYd" user "F.Courtyard")
\t\t(29 "B.CrtYd" user "B.Courtyard")
\t\t(35 "F.Fab" user)
\t\t(33 "B.Fab" user)
\t)
\t(setup
\t\t(pad_to_mask_clearance 0)
\t)
'''


def uid():
    return str(uuid.uuid4())


def load_footprint_block(fp_id: str) -> str:
    libname, name = fp_id.split(":", 1)
    if libname == "auto_flight":
        path = CUSTOM_FP_DIR / f"{name}.kicad_mod"
    else:
        path = STD_FP_DIR / f"{libname}.pretty" / f"{name}.kicad_mod"
    text = path.read_text()
    idx = text.index('(footprint "')
    block = find_balanced(text, idx)
    return block.replace(f'"{name}"', f'"{fp_id}"', 1)


def set_pad_nets(fp_block: str, pad_net_map: dict) -> str:
    """pad_net_map: {pad_number(str): (net_id(int), net_name(str))}."""
    out = []
    idx = 0
    for m in re.finditer(r'\(pad\s+"[^"]*"\s+\S+\s+\S+', fp_block):
        if m.start() < idx:
            continue
        out.append(fp_block[idx:m.start()])
        orig_pblock = find_balanced(fp_block, m.start())
        num = re.match(r'\(pad\s+"([^"]*)"', orig_pblock).group(1)
        pblock = orig_pblock
        if num in pad_net_map:
            net_id, net_name = pad_net_map[num]
            pblock = pblock[:-1] + f'(net {net_id} "{net_name}"))'
        out.append(pblock)
        idx = m.start() + len(orig_pblock)
    out.append(fp_block[idx:])
    return "".join(out)


def build(board_dir: Path, stem: str):
    parts = json.loads((board_dir / f"{stem}.parts.json").read_text())
    nets = json.loads((board_dir / f"{stem}.nets.json").read_text())
    generated_pin_numbers = json.loads((HERE / "generated_pin_numbers.json").read_text())

    real_parts = sorted((p for p in parts if p["kind"] == "part"), key=lambda p: p["title"])
    board = next(p for p in parts if p["kind"] == "board")
    board_w = float(board["properties"]["width"])
    board_h = float(board["properties"]["height"])

    # net_name -> id, skipping singleton nets (no ratsnest needed for those)
    net_ids = {}
    for name, members in nets.items():
        if len(members) >= 2:
            net_ids[name] = len(net_ids) + 1

    # (model_index, connector_id) -> net_id, net_name
    pin_to_net = {}
    for name, members in nets.items():
        if name not in net_ids:
            continue
        for m in members:
            pin_to_net[(m["model_index"], m["connector_id"])] = (net_ids[name], name)

    # Uniform grid sized generously for the largest footprint (the Heltec
    # module, ~46x24mm), independent of the board's own outline. The plane's
    # board is only 71x93mm with 22 real parts -- far too dense to place
    # without overlap using generic header footprints, and the original
    # design physically spreads across multiple daughterboards anyway (see
    # tools/README.md), so the grid is allowed to extend past Edge.Cuts;
    # the user repositions parts to their actual enclosure layout.
    # Some real footprints (Module:Arduino_Nano) have their origin at a
    # corner rather than centered, so placing at a cell's center can still
    # overlap a neighboring cell. Placing at each cell's corner (with a
    # margin) and sizing cells to comfortably exceed any single footprint's
    # extent (Heltec ~46x24mm, Arduino_Nano ~15x36mm) avoids that regardless
    # of a given footprint's own origin convention.
    # Generous enough that a centered-origin footprint (e.g. the Heltec,
    # whose pins run +-21.59mm from its own placement point) placed in the
    # first row/column still clears the board edge.
    margin = 30.0
    n_real = len([p for p in real_parts if CATALOG[p["module_id_ref"]].get("footprint")])
    n_cols = max(1, round(n_real ** 0.5))
    col_w = 55.0
    row_h = 50.0

    fp_chunks = []
    fp_cache = {}
    for i, part in enumerate(real_parts):
        spec = CATALOG[part["module_id_ref"]]
        footprint_id = spec.get("footprint")
        if not footprint_id:
            continue
        if footprint_id not in fp_cache:
            fp_cache[footprint_id] = load_footprint_block(footprint_id)
        base_block = fp_cache[footprint_id]

        pad_fn = spec.get("footprint_pad_number")
        pad_map = {}
        if pad_fn:
            for p in part["pins"]:
                pad_map[p["connector_id"]] = pad_fn(p["name"])
        elif spec["kind"] == "generated":
            pad_map = generated_pin_numbers[part["module_id_ref"]]
        else:
            fn = spec["pin_number"]
            for p in part["pins"]:
                pad_map[p["connector_id"]] = fn(p["name"])

        pad_net_map = {}
        for connector_id, pad_number in pad_map.items():
            key = (part["model_index"], connector_id)
            if key in pin_to_net:
                pad_net_map[pad_number] = pin_to_net[key]

        block = set_pad_nets(base_block, pad_net_map)

        col, row = i % n_cols, i // n_cols
        fx = round(margin + col * col_w + 5, 2)
        fy = round(margin + row * row_h + 5, 2)
        ref = part["title"].replace(" ", "_")

        block = block.replace(
            f'(footprint "{footprint_id}"',
            f'(footprint "{footprint_id}"\n\t(at {fx} {fy} 0)\n\t(uuid "{uid()}")',
            1,
        )
        # Give every fp_text/pad/line/property a fresh uuid so multiple
        # instances of the same footprint don't collide.
        block = re.sub(r'\(uuid "[^"]*"\)', lambda m: f'(uuid "{uid()}")', block)
        block = re.sub(
            r'\(property "Reference" "[^"]*"',
            f'(property "Reference" "{ref}"',
            block,
        )
        fp_chunks.append(block + "\n")

    net_lines = "".join(f'\t(net {nid} "{name}")\n' for name, nid in sorted(net_ids.items(), key=lambda kv: kv[1]))
    net_lines = '\t(net 0 "")\n' + net_lines

    # Edge.Cuts is sized to whichever is larger: the Fritzing source's
    # declared board, or the actual grid footprint -- when the grid is
    # bigger (e.g. the plane: 22 parts, generic header footprints, on a
    # declared 71x93mm board that was never going to hold all of them at
    # once; the real design spans multiple physical daughterboards, see
    # tools/README.md), growing the outline to match keeps DRC's copper/
    # silk-vs-edge checks meaningful instead of flagging nearly everything.
    n_rows = (n_real + n_cols - 1) // n_cols
    grid_w = n_cols * col_w + 2 * margin
    grid_h = n_rows * row_h + 2 * margin
    outline_w = max(board_w, grid_w)
    outline_h = max(board_h, grid_h)

    edge = (
        f'\t(gr_rect (start 0 0) (end {outline_w} {outline_h})\n'
        f'\t\t(stroke (width 0.1) (type default)) (fill none) (layer "Edge.Cuts")\n'
        f'\t\t(uuid "{uid()}")\n\t)\n'
    )

    out = [HEADER, net_lines, edge, "".join(fp_chunks), ")\n"]
    return "".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("board_dir", type=Path)
    ap.add_argument("stem")
    ap.add_argument("project_name")
    args = ap.parse_args()

    text = build(args.board_dir, args.stem)
    out_file = args.board_dir / f"{args.project_name}.kicad_pcb"
    out_file.write_text(text)
    print(f"wrote {out_file}")

    subprocess.run(["kicad-cli", "pcb", "upgrade", "--force", str(out_file)], check=True)
    print("normalized via kicad-cli pcb upgrade")


if __name__ == "__main__":
    main()
