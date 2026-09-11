#!/usr/bin/env python3
"""Extract a clean part list + netlist from a Fritzing .fzz sketch.

Reads the sketch's .fz XML, resolves pin names from embedded/standalone
Fritzing part (.fzp) definitions, and collapses the wire-segment graph into
nets. Emits <name>.parts.json and <name>.nets.json, and prints a summary for
manual auditing against the schematic SVG / README before the result is
trusted by the KiCad generators.
"""
import argparse
import json
import math
import sys
import zipfile
import xml.etree.ElementTree as ET
from pathlib import Path

WIRE_MODULE = "WireModuleID"
BOARD_MODULE = "TwoLayerRectanglePCBModuleID"
NET_LAYERS = {"schematicTrace", "schematic"}


def kind_of(module_id_ref: str) -> str:
    if module_id_ref == WIRE_MODULE:
        return "wire"
    if module_id_ref.startswith("Breadboard"):
        return "breadboard"
    if module_id_ref == BOARD_MODULE:
        return "board"
    return "part"


class UnionFind:
    def __init__(self):
        self.parent = {}

    def find(self, x):
        self.parent.setdefault(x, x)
        while self.parent[x] != x:
            self.parent[x] = self.parent[self.parent[x]]
            x = self.parent[x]
        return x

    def union(self, a, b):
        ra, rb = self.find(a), self.find(b)
        if ra != rb:
            self.parent[ra] = rb


def parse_fzp_connectors(fzp_bytes: bytes) -> dict:
    """id -> {"name": ..., "description": ..., "type": ...}"""
    root = ET.fromstring(fzp_bytes)
    out = {}
    for conn in root.findall("./connectors/connector"):
        cid = conn.get("id")
        desc_el = conn.find("description")
        out[cid] = {
            "name": conn.get("name"),
            "description": (desc_el.text or "").strip() if desc_el is not None else None,
            "type": conn.get("type"),
        }
    return out


def load_fzp_registry(zf: zipfile.ZipFile, parts_dirs: list) -> dict:
    """moduleId -> connectors dict, from parts embedded in the .fzz plus any
    standalone .fzpz or raw .fzp files found in parts_dirs."""
    registry = {}
    for name in zf.namelist():
        if name.startswith("part.") and name.endswith(".fzp"):
            data = zf.read(name)
            root = ET.fromstring(data)
            module_id = root.get("moduleId")
            if module_id:
                registry[module_id] = parse_fzp_connectors(data)

    for parts_dir in parts_dirs or []:
        if not parts_dir or not parts_dir.is_dir():
            continue
        for fzp in sorted(parts_dir.glob("*.fzp")):
            data = fzp.read_bytes()
            root = ET.fromstring(data)
            module_id = root.get("moduleId")
            if module_id and module_id not in registry:
                registry[module_id] = parse_fzp_connectors(data)
        for fzpz in sorted(parts_dir.glob("*.fzpz")):
            try:
                with zipfile.ZipFile(fzpz) as pz:
                    for name in pz.namelist():
                        if name.startswith("part.") and name.endswith(".fzp"):
                            data = pz.read(name)
                            root = ET.fromstring(data)
                            module_id = root.get("moduleId")
                            if module_id and module_id not in registry:
                                registry[module_id] = parse_fzp_connectors(data)
            except zipfile.BadZipFile:
                continue
    return registry


def decode_transform(view_el):
    """Return (rotation_deg, mirrored) from a view's <geometry><transform>,
    or (0, False) if absent. Assumes pure 90-degree-multiple rotations, which
    is all that's observed in these sketches."""
    if view_el is None:
        return 0, False
    t = view_el.find("./geometry/transform")
    if t is None:
        return 0, False
    m11, m12 = float(t.get("m11", 1)), float(t.get("m12", 0))
    m21, m22 = float(t.get("m21", 0)), float(t.get("m22", 1))
    angle = math.degrees(math.atan2(m21, m11))
    angle = round(angle / 90.0) * 90.0 % 360.0
    det = m11 * m22 - m12 * m21
    mirrored = det < 0
    return angle, mirrored


def view_pos(view_el):
    if view_el is None:
        return None
    g = view_el.find("./geometry")
    if g is None:
        return None
    return {"x": float(g.get("x", 0)), "y": float(g.get("y", 0))}


def extract(fzz_path: Path, parts_dirs: list, out_dir: Path, stem: str, overrides_path: Path = None):
    with zipfile.ZipFile(fzz_path) as zf:
        fz_name = next(n for n in zf.namelist() if n.endswith(".fz"))
        sketch = ET.fromstring(zf.read(fz_name))
        fzp_registry = load_fzp_registry(zf, parts_dirs)

    # Manual pin-name overrides for moduleIdRefs with no .fzp anywhere
    # (neither embedded, nor a standalone .fzpz, nor Fritzing's own core
    # library): names inferred from netlist context (which real component
    # pins each connector shares a net with) during the Phase-0 audit, not
    # from a datasheet lookup — see tools/README.md.
    overrides = {}
    if overrides_path and overrides_path.is_file():
        overrides = json.loads(overrides_path.read_text())
    for module_id, pins in overrides.items():
        conns = fzp_registry.setdefault(module_id, {})
        for cid, name in pins.items():
            conns.setdefault(cid, {"name": name, "description": name, "type": None})

    instances = {}
    board = None
    for inst in sketch.findall("./instances/instance"):
        model_index = int(inst.get("modelIndex"))
        module_id_ref = inst.get("moduleIdRef")
        title_el = inst.find("title")
        title = title_el.text.strip() if title_el is not None and title_el.text else module_id_ref
        kind = kind_of(module_id_ref)

        props = {p.get("name"): p.get("value") for p in inst.findall("property")}

        views = inst.find("views")
        sch_view = views.find("schematicView") if views is not None else None
        pcb_view = views.find("pcbView") if views is not None else None

        sch_rot, sch_mirror = decode_transform(sch_view)
        pcb_rot, pcb_mirror = decode_transform(pcb_view)

        connectors = {}
        if sch_view is not None:
            for conn in sch_view.findall("./connectors/connector"):
                connectors[conn.get("connectorId")] = conn

        instances[model_index] = {
            "model_index": model_index,
            "module_id_ref": module_id_ref,
            "title": title,
            "kind": kind,
            "properties": props,
            "schematic_pos": view_pos(sch_view),
            "schematic_rotation": sch_rot,
            "schematic_mirror": sch_mirror,
            "pcb_pos": view_pos(pcb_view),
            "pcb_rotation": pcb_rot,
            "pcb_mirror": pcb_mirror,
            "connector_els": connectors,
        }
        if kind == "board":
            board = instances[model_index]

    # Union-find over (modelIndex, connectorId) using only real schematic-net
    # layers; every schematicView connector of a real part or wire instance
    # participates so wire chains merge correctly. Breadboard/board instances
    # are excluded even though Fritzing emits bookkeeping-only schematicView
    # blocks for them (mirroring breadboard-view hole assignments with
    # layer="schematic" connects that look like real connections but aren't
    # actually wired/placed in the schematic view).
    uf = UnionFind()
    for inst in instances.values():
        if inst["kind"] in ("breadboard", "board"):
            continue
        for cid, conn_el in inst["connector_els"].items():
            node = (inst["model_index"], cid)
            uf.find(node)  # register even if isolated
            for connect in conn_el.findall("./connects/connect"):
                if connect.get("layer") not in NET_LAYERS:
                    continue
                other = (int(connect.get("modelIndex")), connect.get("connectorId"))
                uf.union(node, other)
        if inst["kind"] == "wire":
            # A wire's two ends (connector0/connector1) are electrically the
            # same node; the XML never says so explicitly (it's only implied
            # by both endpoints belonging to the same WireModuleID instance),
            # so without this, wire chains never actually bridge real parts.
            cids = list(inst["connector_els"].keys())
            for cid in cids[1:]:
                uf.union((inst["model_index"], cids[0]), (inst["model_index"], cid))

    groups = {}
    for inst in instances.values():
        for cid in inst["connector_els"]:
            node = (inst["model_index"], cid)
            groups.setdefault(uf.find(node), []).append(node)

    def pin_info(model_index, connector_id):
        inst = instances[model_index]
        conns = fzp_registry.get(inst["module_id_ref"])
        if conns and connector_id in conns:
            info = conns[connector_id]
            return info["name"], info["description"]
        return None, None

    nets = {}
    unresolved_core_parts = set()
    for root_node, members in groups.items():
        real_members = []
        for model_index, cid in members:
            inst = instances[model_index]
            if inst["kind"] != "part":
                continue
            name, desc = pin_info(model_index, cid)
            if name is None and inst["module_id_ref"] not in fzp_registry:
                unresolved_core_parts.add(inst["module_id_ref"])
            real_members.append({
                "instance_title": inst["title"],
                "model_index": model_index,
                "connector_id": cid,
                "pin_name": name,
                "pin_description": desc,
            })
        if not real_members:
            continue
        descs = {m["pin_description"] for m in real_members if m["pin_description"]}
        if len(descs) == 1:
            net_name = list(descs)[0].upper().replace(" ", "_")
        else:
            # No unanimous description (e.g. a transistor's "C" pin sharing
            # a net with everyone else's "GND"): fall back to the most
            # common description among the members instead of always
            # synthesizing a placeholder name, so genuinely-named power/bus
            # nets (GND, VCC, ...) stay readable even with one dissenting
            # label. Only synthesize when there's no majority at all.
            from collections import Counter
            desc_counts = Counter(m["pin_description"] for m in real_members if m["pin_description"])
            top_desc, top_n = (desc_counts.most_common(1) or [(None, 0)])[0]
            if top_desc and top_n > 1 and top_n >= len(real_members) / 2:
                net_name = top_desc.upper().replace(" ", "_")
            else:
                first = real_members[0]
                net_name = f"NET_{first['model_index']}_{first['connector_id']}"
                if len(real_members) > 1:
                    net_name += "_MULTI" if len(descs) > 1 else "_UNNAMED"
        nets.setdefault(net_name, [])
        nets[net_name].append(real_members)

    # flatten (a net_name could theoretically collide across groups; keep unique)
    flat_nets = {}
    for name, groups_list in nets.items():
        if len(groups_list) == 1:
            flat_nets[name] = groups_list[0]
        else:
            for i, g in enumerate(groups_list):
                flat_nets[f"{name}_{i}"] = g

    parts_out = []
    for inst in instances.values():
        if inst["kind"] not in ("part", "board"):
            continue
        pins = []
        conns = fzp_registry.get(inst["module_id_ref"], {})
        for cid in inst["connector_els"]:
            info = conns.get(cid, {})
            pins.append({
                "connector_id": cid,
                "name": info.get("name"),
                "description": info.get("description"),
            })
        parts_out.append({
            "title": inst["title"],
            "module_id_ref": inst["module_id_ref"],
            "model_index": inst["model_index"],
            "kind": inst["kind"],
            "properties": inst["properties"],
            "part_source": (
                "manual_override" if inst["module_id_ref"] in overrides
                else "embedded_or_fzpz" if inst["module_id_ref"] in fzp_registry
                else "core_part_no_fzp"
            ),
            "schematic": {
                "pos": inst["schematic_pos"],
                "rotation_deg": inst["schematic_rotation"],
                "mirrored": inst["schematic_mirror"],
            },
            "pcb": {
                "pos": inst["pcb_pos"],
                "rotation_deg": inst["pcb_rotation"],
                "mirrored": inst["pcb_mirror"],
            },
            "pins": pins,
        })

    out_dir.mkdir(parents=True, exist_ok=True)
    parts_path = out_dir / f"{stem}.parts.json"
    nets_path = out_dir / f"{stem}.nets.json"
    parts_path.write_text(json.dumps(parts_out, indent=2))
    nets_path.write_text(json.dumps(flat_nets, indent=2))

    # ---- human-readable summary ----
    print(f"=== {fzz_path.name} ===")
    real_parts = [p for p in parts_out if p["kind"] == "part"]
    print(f"{len(real_parts)} real part instances, {len(flat_nets)} nets, "
          f"{len(instances) - len(parts_out)} wires/breadboards discarded")
    print()
    print("-- parts --")
    for p in sorted(real_parts, key=lambda p: p["title"]):
        n_pins = len(p["pins"])
        print(f"  [{p['model_index']:>6}] {p['title']:<24} {p['module_id_ref']:<55} "
              f"({p['part_source']}, {n_pins} pins)")
    if board:
        w = board["properties"].get("width")
        h = board["properties"].get("height")
        print(f"  board: {board['title']} {w}x{h} mm")
    print()
    print("-- nets --")
    for name, members in sorted(flat_nets.items()):
        member_str = ", ".join(f"{m['instance_title']}.{m['pin_name'] or m['connector_id']}" for m in members)
        print(f"  {name}: {member_str}")
    if unresolved_core_parts:
        print()
        print("-- WARNING: no .fzp found for these moduleIdRefs (pin names unresolved) --")
        for m in sorted(unresolved_core_parts):
            print(f"  {m}")
    print()
    print(f"wrote {parts_path}")
    print(f"wrote {nets_path}")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("fzz", type=Path, help="path to a .fzz sketch")
    ap.add_argument("--parts-dir", type=Path, action="append", default=[],
                     help="directory containing standalone .fzpz/.fzp parts "
                          "(repeatable)")
    ap.add_argument("--out-dir", type=Path, default=Path("."),
                     help="directory to write <stem>.parts.json / <stem>.nets.json")
    ap.add_argument("--stem", default=None, help="output filename stem (default: fzz basename)")
    ap.add_argument("--pin-overrides", type=Path, default=None,
                     help="JSON file of {moduleIdRef: {connectorId: name}} for parts with no .fzp anywhere")
    args = ap.parse_args()

    stem = args.stem or args.fzz.stem
    extract(args.fzz, args.parts_dir, args.out_dir, stem, args.pin_overrides)


if __name__ == "__main__":
    main()
