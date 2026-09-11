"""Maps each Fritzing moduleIdRef used in the two sketches to a KiCad symbol
and footprint, and provides the pin-name -> pin-number resolution needed to
wire a generated schematic symbol's pins to the extracted netlist.

Two families:
  - STANDARD: reuse an existing KiCad library symbol. `pin_number(name)`
    resolves one of our extracted Fritzing pin names to that symbol's pin
    number (the two namespaces differ, see per-entry `pin_map`).
  - GENERATED: no suitable standard symbol exists (or the standard part's
    pin count doesn't match what this breakout board actually exposes, e.g.
    a bare-die sensor symbol vs. a breakout board's 4 exposed pins) -> we
    synthesize a simple box+pins symbol directly from parts.json's resolved
    pin list (gen_symbols.py) and pair it with a generic pin-header
    footprint (or, for the Heltec module, a generated dual-row footprint).
"""
import re

CUSTOM_LIB = "auto_flight"


def _arduino_nano_footprint_pad(fritzing_name: str, _seen={}) -> str:
    """Module:Arduino_Nano's real footprint pads are numbered to match the
    *real* MCU_Module:Arduino_Nano_v2.x/v3.x symbol pin table -- unrelated to
    our generated Arduino_Nano symbol's pin numbers (assigned by Fritzing
    connectorId order). Bridges Fritzing pin name -> that real pad number."""
    table = {
        "D1/TX": 1, "D0/RX": 2, "D2": 5, "D3": 6, "D4": 7, "D5": 8, "D6": 9,
        "D7": 10, "D8": 11, "D9": 12, "D10": 13, "D11/MOSI": 14, "D12/MISO": 15,
        "D13/SCK": 16, "3V3": 17, "AREF": 18, "A0": 19, "A1": 20, "A2": 21,
        "A3": 22, "A4": 23, "A5": 24, "A6": 25, "A7": 26, "5V": 27, "VIN": 30,
    }
    name = fritzing_name.strip()
    if name.upper() in ("GND", "RESET"):
        key = f"__{name.upper()}_count"
        n = _seen.get(key, 0)
        _seen[key] = n + 1
        if name.upper() == "GND":
            return str(4 if n == 0 else 29)
        return str(3 if n == 0 else 28)
    return str(table[name])


def transistor_pin_number(fritzing_name: str) -> str:
    # SparkFun-DiscreteSemi-TRANSISTOR_NPN_2-PTH's own connector names are
    # already literally "B"/"E"/"C" (a real fzp, not inferred) and match
    # Device Q_NPN_BEC's pin names 1:1.
    return {"B": "1", "E": "2", "C": "3"}[fritzing_name.strip().upper()]


# module_id_ref -> spec
CATALOG = {
    "5BandResistorModuleID": {
        "kind": "standard", "symbol": "Device:R", "ref": "R",
        "footprint": "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal",
        "pin_number": lambda name: {"PIN 0": "1", "PIN 1": "2"}[name.strip().upper()],
    },
    "SparkFun-DiscreteSemi-TRANSISTOR_NPN_2-PTH": {
        "kind": "standard", "symbol": "Transistor_BJT:Q_NPN_BEC", "ref": "Q",
        "footprint": "Package_TO_SOT_THT:TO-92_Inline",
        "pin_number": transistor_pin_number,
    },
    "Arduino Nano3(fix)": {
        # A generated box symbol rather than MCU_Module:Arduino_Nano_v3.x:
        # that standard symbol "extends" a base unit (Arduino_Nano_v2.x),
        # which would need its own lib_symbols entry + renamed sub-units to
        # embed correctly -- not worth the complexity here. The real,
        # purpose-built "Module:Arduino_Nano" footprint is used regardless.
        "kind": "generated", "symbol": f"{CUSTOM_LIB}:Arduino_Nano", "ref": "A",
        "footprint": "Module:Arduino_Nano",
        "footprint_pad_number": _arduino_nano_footprint_pad,
    },
    # Generic connectors: reinterpreted per the Phase-0 audit (see
    # tools/README.md) rather than modelled as their Fritzing part shape.
    "sbus_receiver_d42a1b7f2dae5e4bdc8f8fe0cce01852_1": {
        "kind": "standard", "symbol": "Connector_Generic:Conn_01x03", "ref": "J",
        "footprint": "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical",
        "pin_number": lambda name: {"GND": "1", "VCC": "2", "SIGNAL": "3"}[name.strip().upper()],
    },
    "Dagu_DGServo_9g_header_male": {
        "kind": "standard", "symbol": "Connector_Generic:Conn_01x03", "ref": "J",
        "footprint": "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical",
        "pin_number": lambda name: {"GND": "1", "VCC": "2", "PULSE": "3"}[name.strip().upper()],
    },
    "SparkFun-Connectors-M04-JST-PTH": {
        "kind": "standard", "symbol": "Connector_Generic:Conn_01x04", "ref": "J",
        "footprint": "Connector_JST:JST_EH_B4B-EH-A_1x04_P2.50mm_Vertical",
        "pin_number": lambda name: name.strip(),  # names are already "1".."4"
    },
    # Generated (custom box+pins symbol from parts.json's resolved pin list;
    # pin NUMBER = 1-based physical order used at symbol-generation time,
    # see gen_symbols.py's pin ordering).
    "GT-U8-GPS-module_1": {
        "kind": "generated", "symbol": f"{CUSTOM_LIB}:GT-U8_GPS_Module", "ref": "U",
        "footprint": "Connector_PinHeader_2.54mm:PinHeader_1x05_P2.54mm_Vertical",
    },
    "Heltec_WiFi_LoRa_32_V3.2": {
        "kind": "generated", "symbol": f"{CUSTOM_LIB}:Heltec_WiFi_LoRa_32_V3_2", "ref": "U",
        "footprint": f"{CUSTOM_LIB}:Heltec_WiFi_LoRa_32_V3.2",
    },
    "I2C_level_converter_444e27d4b4b231371fe2ccfc428902c1_1": {
        # Real part is a 12-pin, 4-channel bidirectional level shifter (HV,
        # GND, HV1-4 / LV, GND, LV1-4); the Fritzing source only ever
        # modeled the 8 pins actually wired (2 of the 4 channels, for I2C
        # SDA/SCL). See EXTRA_PINS in gen_symbols.py for the 4 unconnected
        # channel pins added to the symbol for physical accuracy.
        "kind": "generated", "symbol": f"{CUSTOM_LIB}:I2C_Level_Converter", "ref": "U",
        # Real physical layout, given directly by the user: 2 rows of 6
        # pins, 2.54mm pitch, 11.5mm row spacing -- no standard KiCad
        # footprint matches that row spacing, so it's custom-generated
        # (gen_footprints.py) rather than a generic single-row header.
        "footprint": f"{CUSTOM_LIB}:I2C_Level_Converter",
    },
    "adafruit_2d459a7ab102a5fe885fef0d2637801e_1_32": {
        "kind": "generated", "symbol": f"{CUSTOM_LIB}:Battery_Fuel_Gauge", "ref": "U",
        "footprint": "Connector_PinHeader_2.54mm:PinHeader_1x06_P2.54mm_Vertical",
    },
    "bmp180_breakout": {
        "kind": "generated", "symbol": f"{CUSTOM_LIB}:BMP280_Breakout", "ref": "U",
        "footprint": "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical",
    },
    "MPU6050_GY521_782354e339f672575bb20992ece4ab1b_9": {
        "kind": "generated", "symbol": f"{CUSTOM_LIB}:MPU6050_Breakout", "ref": "U",
        "footprint": "Connector_PinHeader_2.54mm:PinHeader_1x08_P2.54mm_Vertical",
    },
    "ADS1115_0c5b9a9f966b84ee9b687c538575febb_1": {
        # 10 pins now (VDD/GND/SCL/SDA/ADDR/ALRT/A0-A3, the real TSSOP-10
        # pinout) -- see gen_symbols.py's EXTRA_PINS for ALRT/A3, which the
        # Fritzing source never wired.
        "kind": "generated", "symbol": f"{CUSTOM_LIB}:ADS1115_Breakout", "ref": "U",
        "footprint": "Connector_PinHeader_2.54mm:PinHeader_1x10_P2.54mm_Vertical",
    },
}

# module_id_refs handled structurally, not as placed symbols.
NON_COMPONENT_KINDS = {"board", "breadboard", "wire"}


def physical_pin_order(pins):
    """Sort a parts.json part's pin list into physical header order. Prefers
    a numeric 'Pin N' / 'pin N' name (GT-U8, Heltec); falls back to the
    numeric suffix of the Fritzing connectorId."""
    def key(p):
        m = re.match(r"^[Pp]in\s*(\d+)$", p["name"] or "")
        if m:
            return (0, int(m.group(1)))
        m = re.match(r"^connector(\d+)$", p["connector_id"])
        return (1, int(m.group(1)) if m else 0)
    return sorted(pins, key=key)
