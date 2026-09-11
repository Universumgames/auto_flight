#!/usr/bin/env python3
"""Generate hardware/circuits/kicad_libs/auto_flight.kicad_sym: simple
box+pins symbols for the parts that have no suitable existing KiCad symbol
(see parts_catalog.py's "generated" entries). Pin data comes straight out of
plane.parts.json / base_station.parts.json (already resolved by
fz_extract.py from the Fritzing part definitions, plus tools/pin_overrides.json
for the two parts with no .fzp anywhere).
"""
import json
import re
from pathlib import Path

from parts_catalog import CATALOG, physical_pin_order

HERE = Path(__file__).parent
LIB_OUT = HERE.parent / "kicad_libs" / "auto_flight.kicad_sym"

PIN_SPACING = 2.54  # mm, standard KiCad grid
PIN_LENGTH = 2.54

POWER_NAMES = re.compile(
    r"^(VCC|VDD|VDDIO|3V3|\+?5V|VIN|HV|LV|GND.*|GND_HV|GND_LV)$", re.I
)


def electrical_type(name: str) -> str:
    return "power_in" if POWER_NAMES.match(name.strip()) else "passive"


def load_part(parts_json: Path, title: str):
    parts = json.loads(parts_json.read_text())
    for p in parts:
        if p["title"] == title:
            return p
    raise KeyError(f"{title} not found in {parts_json}")


def find_generated_part(module_id_ref: str, sources):
    for parts_json in sources:
        parts = json.loads(parts_json.read_text())
        for p in parts:
            if p["module_id_ref"] == module_id_ref:
                return p
    raise KeyError(f"{module_id_ref} not found in any of {sources}")


def sexpr_pin(number, name, etype, y, side):
    angle = 0 if side == "left" else 180
    x = -12.7 if side == "left" else 12.7
    name_disp = name if name else "~"
    return (
        f'\t\t\t(pin {etype} line\n'
        f'\t\t\t\t(at {x} {y} {angle})\n'
        f'\t\t\t\t(length {PIN_LENGTH})\n'
        f'\t\t\t\t(name "{name_disp}" (effects (font (size 1.27 1.27))))\n'
        f'\t\t\t\t(number "{number}" (effects (font (size 1.27 1.27))))\n'
        f'\t\t\t)\n'
    )


def gen_symbol_sexpr(entry_name: str, ref_prefix: str, pins, description: str):
    """pins: ordered list of (number:str, name:str)."""
    n = len(pins)
    left_n = (n + 1) // 2
    right_n = n - left_n
    left_pins = pins[:left_n]
    right_pins = pins[left_n:]

    body_half_h = max(left_n, right_n, 1) * PIN_SPACING / 2 + PIN_SPACING / 2
    body_half_w = 10.16  # 4 grid units

    out = [f'\t(symbol "{entry_name}"\n']
    out.append('\t\t(pin_names (offset 1.016))\n')
    out.append('\t\t(exclude_from_sim no)\n\t\t(in_bom yes)\n\t\t(on_board yes)\n')
    out.append(f'\t\t(property "Reference" "{ref_prefix}" (at {-body_half_w} {body_half_h + 2.54} 0)'
                f' (effects (font (size 1.27 1.27)) (justify left)))\n')
    out.append(f'\t\t(property "Value" "{entry_name}" (at {-body_half_w} {body_half_h + 1.27} 0)'
                f' (effects (font (size 1.27 1.27)) (justify left)))\n')
    out.append('\t\t(property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) hide))\n')
    out.append('\t\t(property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) hide))\n')
    out.append(f'\t\t(property "Description" "{description}" (at 0 0 0) (effects (font (size 1.27 1.27)) hide))\n')

    out.append(f'\t\t(symbol "{entry_name}_0_1"\n')
    out.append(f'\t\t\t(rectangle (start {-body_half_w} {body_half_h}) (end {body_half_w} {-body_half_h})\n')
    out.append('\t\t\t\t(stroke (width 0.254) (type default))\n')
    out.append('\t\t\t\t(fill (type background))\n\t\t\t)\n')
    out.append('\t\t)\n')

    out.append(f'\t\t(symbol "{entry_name}_1_1"\n')
    top_l = (left_n - 1) * PIN_SPACING / 2
    for i, (num, name) in enumerate(left_pins):
        y = top_l - i * PIN_SPACING
        out.append(sexpr_pin(num, name, electrical_type(name), y, "left"))
    top_r = (right_n - 1) * PIN_SPACING / 2
    for i, (num, name) in enumerate(right_pins):
        y = top_r - i * PIN_SPACING
        out.append(sexpr_pin(num, name, electrical_type(name), y, "right"))
    out.append('\t\t)\n')
    out.append('\t\t(embedded_fonts no)\n')
    out.append('\t)\n')
    return "".join(out)


# Pins the Fritzing source never modeled (no connectorId at all, so they
# can't come from parts.json) but that exist on the real physical part --
# added for a physically-accurate symbol. Genuinely unconnected: no entry
# here ever reaches generated_pin_numbers.json, so gen_schematic.py/gen_pcb.py
# just leave them unwired, matching reality (those channels aren't used).
EXTRA_PINS = {
    "I2C_level_converter_444e27d4b4b231371fe2ccfc428902c1_1": ["HV3", "HV4", "LV3", "LV4"],
    # ADS1115's real fzp (embedded in schematics_plane.fzz) declares 10
    # connectors (connector8-17: VDD/GND/SCL/SDA/ADDR/ALRT/A0/A1/A2/A3) --
    # a full TSSOP-10 pinout -- but the placed instance's schematicView only
    # ever had connector elements for 8 of them; connector13 (ALRT) and
    # connector17 (A3) are simply missing from the instance, so they never
    # reach parts.json regardless of pin_overrides.json (that only renames
    # connectors fz_extract.py already found, it can't add ones the
    # instance never declared).
    "ADS1115_0c5b9a9f966b84ee9b687c538575febb_1": ["ALRT", "A3"],
}

# Override physical_pin_order()'s generic connectorId-based ordering where
# grouping pins by function (all HV-side together, then all LV-side) reads
# far better on the symbol than connectorId order does. Matched by identity
# -- connectorId for real pins, the label itself for EXTRA_PINS entries
# (which have no connectorId) -- rather than by label for every pin, since
# both GND pins now carry the literal label "GND" and would otherwise
# collide as dict keys. Anything not listed here keeps its
# physical_pin_order position, appended after the matched ones.
#
# Real physical layout given directly by the user: one row HV1, HV2, HV,
# GND, HV3, HV4; the other row LV1, LV2, LV, GND, LV3, LV4 -- with channel
# numbers matching directly across (HV1 shifts to LV1, etc; HV1/LV1 is the
# channel actually wired to SCL here, HV2/LV2 to SDA -- see pin_overrides.json).
PIN_ORDER_OVERRIDE = {
    "I2C_level_converter_444e27d4b4b231371fe2ccfc428902c1_1":
        ["connector4", "connector5", "connector3", "connector2", "HV3", "HV4",
         "connector7", "connector6", "connector8", "connector9", "LV3", "LV4"],
    # Real 10-pin TSSOP order given directly by the user: VDD, GND, SCL,
    # SDA, ADDR, ALRT, A0, A1, A2, A3 (connector8-12,14-16 are the 8 real
    # pins already in parts.json; ALRT/A3 are the two EXTRA_PINS above).
    "ADS1115_0c5b9a9f966b84ee9b687c538575febb_1":
        ["connector8", "connector9", "connector10", "connector11", "connector12", "ALRT",
         "connector14", "connector15", "connector16", "A3"],
}


def main():
    plane_json = HERE.parent / "plane_pcb" / "plane.parts.json"
    base_json = HERE.parent / "base_station_pcb" / "base_station.parts.json"
    sources = [plane_json, base_json]

    generated = {mid: spec for mid, spec in CATALOG.items() if spec["kind"] == "generated"}

    descriptions = {
        "GT-U8-GPS-module_1": "GT-U8 GPS module breakout (5-pin: VCC/GND/TX/RX/PPS)",
        "Heltec_WiFi_LoRa_32_V3.2": "Heltec WiFi LoRa 32 V3.2 dev board (ESP32-S3 + SX1262), 36-pin dual header",
        "I2C_level_converter_444e27d4b4b231371fe2ccfc428902c1_1":
            "Generic 4-channel bidirectional logic-level converter breakout, 12 pins "
            "(HV/GND/HV1-4, LV/GND/LV1-4); only 2 of 4 channels used, for I2C SDA/SCL. "
            "Used-pin names inferred from netlist, see tools/README.md",
        "adafruit_2d459a7ab102a5fe885fef0d2637801e_1_32": "Adafruit I2C battery fuel-gauge/balancer breakout",
        "bmp180_breakout": "BMP280 barometer breakout (4-pin I2C: VCC/GND/SCL/SDA)",
        "MPU6050_GY521_782354e339f672575bb20992ece4ab1b_9": "MPU6050 IMU breakout (GY-521 style, pin names inferred from netlist)",
        "ADS1115_0c5b9a9f966b84ee9b687c538575febb_1":
            "ADS1115 4-channel ADC breakout, 10 pins (VDD/GND/SCL/SDA/ADDR/ALRT/A0-A3); "
            "ALRT and A3 are on the real part but weren't wired in the Fritzing source",
        "Arduino Nano3(fix)": "Arduino Nano (generic pinout box; footprint is the real Module:Arduino_Nano)",
    }

    chunks = []
    pin_number_maps = {}
    for module_id_ref, spec in generated.items():
        part = find_generated_part(module_id_ref, sources)
        ordered = physical_pin_order(part["pins"])

        # (connector_id_or_None, label) pairs, physical/connectorId order first
        entries = []
        for p in ordered:
            label = (p["description"] or p["name"] or "").split(",")[0].strip() or p["name"]
            entries.append((p["connector_id"], label))
        for extra_label in EXTRA_PINS.get(module_id_ref, []):
            entries.append((None, extra_label))

        override = PIN_ORDER_OVERRIDE.get(module_id_ref)
        if override:
            # key by connectorId when present (unique per Fritzing pin, even
            # when two pins share a label like "GND"), else by the label
            # itself (EXTRA_PINS entries, which have no connectorId).
            by_key = {(cid if cid is not None else label): (cid, label) for cid, label in entries}
            entries = [by_key.pop(key) for key in override if key in by_key]
            entries += list(by_key.values())  # anything unlisted, appended as-is

        pins = []
        conn_to_number = {}
        for i, (connector_id, label) in enumerate(entries):
            number = str(i + 1)
            pins.append((number, label))
            if connector_id is not None:
                conn_to_number[connector_id] = number
        pin_number_maps[module_id_ref] = conn_to_number
        entry_name = spec["symbol"].split(":", 1)[1]
        chunks.append(gen_symbol_sexpr(entry_name, spec["ref"], pins, descriptions[module_id_ref]))
        print(f"{entry_name}: {len(pins)} pins -> {[p[1] for p in pins]}")

    (HERE / "generated_pin_numbers.json").write_text(json.dumps(pin_number_maps, indent=2))

    lib = "(kicad_symbol_lib\n\t(version 20231120)\n\t(generator \"auto_flight_conversion\")\n\t(generator_version \"1.0\")\n"
    lib += "".join(chunks)
    lib += ")\n"
    LIB_OUT.parent.mkdir(parents=True, exist_ok=True)
    LIB_OUT.write_text(lib)
    print(f"\nwrote {LIB_OUT}")

    import subprocess
    tmp = LIB_OUT.with_suffix(".tmp.kicad_sym")
    subprocess.run(["kicad-cli", "sym", "upgrade", str(LIB_OUT), "--output", str(tmp)], check=True)
    tmp.replace(LIB_OUT)
    print("normalized via kicad-cli sym upgrade")


if __name__ == "__main__":
    main()
