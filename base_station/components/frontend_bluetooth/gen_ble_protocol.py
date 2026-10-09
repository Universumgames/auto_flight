#!/usr/bin/env python3
"""Generates the BLE wire-contract sources from ble_protocol.json.

The topic table itself is not listed in the JSON: it mirrors enum PacketType from
base_station/shared_components/flight_com/packets/base.hpp (see the schema's
"topics" entry), so adding a packet type automatically adds a BLE topic.

This is the single source of truth for the BLE topic table (byte value ->
characteristic UUID), the service UUID, the application-level fragmentation
header layout, and the scan-response manufacturer-data layout shared between
the base station (C++/NimBLE) and its clients (iOS/CoreBluetooth today,
Android/KMP once it grows a BLE transport). Nothing here should be hand-edited
in the generated output files - edit ble_protocol.json and regenerate.

Usage:
    # Regenerate the C++ header (normally done automatically by CMake at build time):
    python3 gen_ble_protocol.py --out-cpp BLETopics.generated.hpp

    # Regenerate the iOS Swift file (wired into an Xcode Run Script build phase):
    python3 gen_ble_protocol.py --out-swift ../../../app/iosApp/iosApp/Models/BluetoothUUIDs.generated.swift

    # Regenerate the Android/KMP Kotlin stub (currently manual - see that file's header):
    python3 gen_ble_protocol.py --out-kotlin ../../../app/sharedLogic/src/commonMain/kotlin/de/universegame/auto_flight/app/wire/BleProtocol.generated.kt

    # Check committed output is up to date with the schema (exits 1 on drift):
    python3 gen_ble_protocol.py --check --out-swift <path> --out-kotlin <path>
"""
import argparse
import json
import re
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_SCHEMA = SCRIPT_DIR / "ble_protocol.json"

GENERATED_NOTICE = (
    "GENERATED FILE - DO NOT EDIT BY HAND.\n"
    "Source of truth: base_station/components/frontend_bluetooth/ble_protocol.json\n"
    "(topics mirror enum PacketType in base_station/shared_components/flight_com/packets/base.hpp)\n"
    "Regenerate with base_station/components/frontend_bluetooth/gen_ble_protocol.py"
)


def load_schema(path: Path) -> dict:
    with open(path, "r", encoding="utf-8") as f:
        raw = json.load(f)
    # "//"-prefixed keys are documentation-only comments (JSON has no comment syntax).
    schema = {k: v for k, v in raw.items() if not k.startswith("//")}
    schema["topicEnum"] = schema["topics"]["enum"]
    schema["topicEnumInclude"] = schema["topics"]["cppInclude"]
    schema["topics"] = resolve_topics(schema["topics"], path.parent)
    return schema


def snake_to_camel(name: str) -> str:
    """SENSOR_UPDATE -> sensorUpdate"""
    first, *rest = name.lower().split("_")
    return first + "".join(part.capitalize() for part in rest)


def parse_cpp_enum(header: Path, enum_name: str) -> list[tuple[str, int, list[str]]]:
    """Returns (name, value, doc lines) for every entry of `enum class <enum_name>` in `header`.

    Only handles what an enum body can reasonably contain here: `NAME = <int literal>`,
    implicit `NAME` (previous value + 1), `///` doc comments and `//` / `/* */` comments.
    """
    text = header.read_text(encoding="utf-8")
    match = re.search(rf"enum\s+class\s+{re.escape(enum_name)}\b[^{{;]*\{{(.*?)\}}", text, re.DOTALL)
    if not match:
        raise SystemExit(f"error: enum class {enum_name} not found in {header}")
    body = re.sub(r"/\*.*?\*/", "", match.group(1), flags=re.DOTALL)

    entries = []
    doc: list[str] = []
    next_value = 0
    for line in body.splitlines():
        line = line.strip()
        if line.startswith("///"):
            doc.append(line[3:].strip())
            continue
        line = line.split("//", 1)[0].strip().rstrip(",").strip()
        if not line:
            doc = []  # a blank / plain-comment line detaches any preceding doc comment
            continue
        entry = re.fullmatch(r"(\w+)\s*(?:=\s*(\w+))?", line)
        if not entry:
            raise SystemExit(f"error: can't parse {enum_name} entry {line!r} in {header}")
        value = int(entry.group(2), 0) if entry.group(2) else next_value
        entries.append((entry.group(1), value, doc))
        next_value = value + 1
        doc = []
    return entries


def resolve_topics(spec: dict, schema_dir: Path) -> list[dict]:
    """Expands the schema's `topics` reference into one topic per entry of the referenced enum."""
    header = (schema_dir / spec["sourceHeader"]).resolve()
    prefix = spec["uuidPrefix"].removeprefix("0x").removeprefix("0X").upper()
    if not re.fullmatch(r"[0-9A-F]{2}", prefix):
        raise SystemExit(f"error: uuidPrefix must be a single hex byte, got {spec['uuidPrefix']!r}")

    topics = []
    for name, value, doc in parse_cpp_enum(header, spec["enum"]):
        if not 0 <= value <= 0xFF:
            raise SystemExit(f"error: {spec['enum']}::{name} = {value} does not fit in one byte")
        topics.append({
            "name": f"BLE_{name}",
            "sourceName": name,
            "value": f"0x{value:02X}",
            "uuid": f"{prefix}{value:02X}",
            "caseName": snake_to_camel(name),
            "doc": doc,
        })
    return topics


def _doc(topic: dict, indent: str) -> str:
    """Renders a topic's doc comment lines as `///` comments (valid in C++, Swift and Kotlin)."""
    return "".join(f"{indent}/// {line}\n" for line in topic["doc"])


def _comment(text: str) -> str:
    """Prefixes every line of `text` with '// ', for embedding doc comments in templates."""
    return "\n".join(f"// {line}" for line in text.split("\n"))


def render_cpp(schema: dict) -> str:
    topics = schema["topics"]
    mfg = schema["manufacturerData"]
    company_id_hex = mfg["companyID"].removeprefix("0x").removeprefix("0X")

    notice = _comment(GENERATED_NOTICE)
    enum_entries = "\n".join(f"{_doc(t, '        ')}        {t['name']} = {t['value']}," for t in topics)
    notify_entries = "\n".join(f"        {t['name']}," for t in topics)
    uuid_entries = "\n".join(f"        BLE_UUID16_INIT(0x{t['uuid']}), // {t['name']}" for t in topics)
    packet_type = schema["topicEnum"]
    to_notify_cases = "\n".join(
        f"            case {packet_type}::{t['sourceName']}: return {t['name']};" for t in topics)
    to_packet_cases = "\n".join(
        f"            case {t['name']}: return {packet_type}::{t['sourceName']};" for t in topics)

    # Templates below are emitted flush-left (ignoring this function's own indentation) so
    # they read as plain C++/Swift/Kotlin instead of a wall of per-line .append() calls.
    return f"""#pragma once
{notice}
//
// Mirrored on iOS by BluetoothUUIDs.generated.swift and (stub, not yet wired
// up on Android) by sharedLogic's wire/BleProtocol.generated.kt - keep all
// three in sync by editing ble_protocol.json and regenerating, never by hand.
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>

#include "host/ble_uuid.h"
#include "{schema['topicEnumInclude']}"

namespace BLETopics {{
    enum NotifyByte : uint8_t {{
{enum_entries}
    }};

    inline constexpr NotifyByte ALL_NOTIFICATIONS[] = {{
{notify_entries}
    }};

    inline constexpr size_t ALL_NOTIFICATIONS_SIZE = std::size(ALL_NOTIFICATIONS);

    // UUIDs, in the same order as ALL_NOTIFICATIONS (looked up by position, not by enum value).
    inline constexpr ble_uuid16_t TOPIC_UUIDS[ALL_NOTIFICATIONS_SIZE] = {{
{uuid_entries}
    }};

    /**
     * Look up the 16-bit UUID registered for a topic.
     * @return Pointer to the matching entry in TOPIC_UUIDS, or nullptr if the
     *         topic has no registered UUID (e.g. an unknown value).
     */
    inline const ble_uuid16_t* getUUIDForTopic(const NotifyByte topic) {{
        for (size_t i = 0; i < ALL_NOTIFICATIONS_SIZE; ++i) {{
            if (ALL_NOTIFICATIONS[i] == topic) {{
                return &TOPIC_UUIDS[i];
            }}
        }}
        return nullptr; // Invalid/unregistered topic
    }}

    /**
     * Map a {packet_type} to the topic carrying it.
     * @return The matching topic, or std::nullopt for a value outside the enum
     *         (e.g. a {packet_type} cast from an unvalidated wire byte).
     */
    inline constexpr std::optional<NotifyByte> toNotifyByte(const {packet_type} type) {{
        switch (type) {{
{to_notify_cases}
        }}
        return std::nullopt;
    }}

    /**
     * Map a topic back to the {packet_type} it carries.
     * @return The matching {packet_type}, or std::nullopt for a value outside the enum
     *         (e.g. a NotifyByte cast from an unvalidated TopicType).
     */
    inline constexpr std::optional<{packet_type}> toPacketType(const NotifyByte topic) {{
        switch (topic) {{
{to_packet_cases}
        }}
        return std::nullopt;
    }}
}}

namespace BLEProtocol {{
    // Default service UUID advertised by this firmware; overridable at build time via
    // `idf.py menuconfig` -> Bluetooth Handler Configuration (CONFIG_BLUETOOTH_SERVICE_UUID).
    // Clients that can't read Kconfig (iOS, and eventually Android) compile this default
    // in directly, so a non-default CONFIG_BLUETOOTH_SERVICE_UUID requires updating
    // ble_protocol.json too.
    inline constexpr uint16_t SERVICE_UUID = 0x{schema['serviceUUID']};

    // Every fragmented notify/write is prefixed with this many bytes: [0:2) totalLength,
    // [2:4) offset, both little-endian uint16. See BluetoothManager::sendFragmented /
    // handleFragmentedWrite.
    inline constexpr uint16_t FRAGMENT_HEADER_SIZE = {schema['fragmentHeaderSize']};

    // Build-version manufacturer data advertised in the scan response: company ID (2
    // bytes LE) + build epoch (N bytes LE). See BluetoothManager::populateBuildVersionMfgData.
    inline constexpr uint16_t MFG_COMPANY_ID = 0x{company_id_hex};
    inline constexpr uint16_t MFG_BUILD_EPOCH_FIELD_SIZE = {mfg['buildEpochFieldSize']};
    inline constexpr uint16_t MFG_DATA_LEN = 2 + MFG_BUILD_EPOCH_FIELD_SIZE;
}}
"""


def render_swift(schema: dict) -> str:
    topics = schema["topics"]
    mfg = schema["manufacturerData"]
    company_id_hex = mfg["companyID"].removeprefix("0x").removeprefix("0X")

    notice = _comment(GENERATED_NOTICE)
    case_entries = "\n".join(f"{_doc(t, '    ')}    case {t['name']} = {t['value']}" for t in topics)
    characteristic_cases = "\n".join(f"            case .{t['name']}: return .{t['caseName']}" for t in topics)
    uuid_cases = "\n".join(f'{_doc(t, "    ")}    case {t["caseName"]} = "{t["uuid"]}"' for t in topics)

    return f"""//
{notice}
//
// Mirrors base_station/components/frontend_bluetooth/BLETopics.generated.hpp.
//

import CoreBluetooth

let ServiceUUID = CBUUID(string: "{schema['serviceUUID']}")

enum BLEProtocolConstants {{
    static let fragmentHeaderSize = {schema['fragmentHeaderSize']}
    static let mfgCompanyID: UInt16 = 0x{company_id_hex}
}}

enum NotifyByte: UInt8, CaseIterable, Identifiable {{
{case_entries}

    var id: UInt8 {{ rawValue }}

    var characteristic: CharacteristicUUID {{
        switch self {{
{characteristic_cases}
        }}
    }}
}}

enum CharacteristicUUID: String, CaseIterable, Identifiable {{
{uuid_cases}
}}

extension CharacteristicUUID {{
    var uuid: CBUUID {{
        return CBUUID(string: rawValue)
    }}

    var id: CBUUID {{
        uuid
    }}
}}
"""


def render_kotlin(schema: dict) -> str:
    topics = schema["topics"]
    mfg = schema["manufacturerData"]
    company_id_hex = mfg["companyID"].removeprefix("0x").removeprefix("0X")

    notice = _comment(GENERATED_NOTICE)
    topic_entries = "\n".join(f'{_doc(t, "    ")}    {t["name"]}({t["value"]}u, "{t["uuid"]}"),' for t in topics)
    packet_type = schema["topicEnum"]
    to_packet_cases = "\n".join(f"            {t['name']} -> {packet_type}.{t['sourceName']}" for t in topics)
    to_topic_cases = "\n".join(f"        {packet_type}.{t['sourceName']} -> BleTopic.{t['name']}" for t in topics)

    return f"""{notice}
//
// STUB: not referenced by any code yet - the Android app currently talks to the
// base station over a websocket (see KtorFlightRepository.kt), not Bluetooth.
// This exists so the day Android/KMP grows a BLE transport, the wire contract is
// already generated from the same source of truth as the C++ and Swift sides
// instead of being hand-copied a third time. Regenerate with
// base_station/components/frontend_bluetooth/gen_ble_protocol.py --out-kotlin <path>
package de.universegame.auto_flight.app.wire

object BleProtocol {{
    const val SERVICE_UUID: String = "{schema['serviceUUID']}"
    const val FRAGMENT_HEADER_SIZE: Int = {schema['fragmentHeaderSize']}
    const val MFG_COMPANY_ID: Int = 0x{company_id_hex}
    const val MFG_BUILD_EPOCH_FIELD_SIZE: Int = {mfg['buildEpochFieldSize']}
    const val MFG_DATA_LEN: Int = 2 + MFG_BUILD_EPOCH_FIELD_SIZE
}}

enum class BleTopic(val value: UByte, val characteristicUUID: String) {{
{topic_entries}
    ;

    /** The [{packet_type}] carried by this topic. */
    val packetType: {packet_type}
        get() = when (this) {{
{to_packet_cases}
        }}
}}

/**
 * The [BleTopic] carrying this packet type. Exhaustive on purpose: if the hand-written
 * [{packet_type}] in FrontendPackets.kt drifts from the C++ enum, this stops compiling.
 */
val {packet_type}.bleTopic: BleTopic
    get() = when (this) {{
{to_topic_cases}
    }}
"""


def write_or_check(content: str, out_path: Path, check: bool) -> bool:
    """Returns True if content is/was up to date (i.e. no error)."""
    if check:
        if not out_path.exists() or out_path.read_text(encoding="utf-8") != content:
            print(f"STALE: {out_path} does not match ble_protocol.json / the mirrored enum - regenerate it", file=sys.stderr)
            return False
        print(f"OK: {out_path} is up to date")
        return True
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(content, encoding="utf-8")
    print(f"Wrote {out_path}")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--schema", type=Path, default=DEFAULT_SCHEMA)
    parser.add_argument("--out-cpp", type=Path)
    parser.add_argument("--out-swift", type=Path)
    parser.add_argument("--out-kotlin", type=Path)
    parser.add_argument("--check", action="store_true",
                         help="Verify existing outputs match the schema instead of writing them.")
    args = parser.parse_args()

    schema = load_schema(args.schema)
    ok = True

    if args.out_cpp:
        ok &= write_or_check(render_cpp(schema), args.out_cpp, args.check)
    if args.out_swift:
        ok &= write_or_check(render_swift(schema), args.out_swift, args.check)
    if args.out_kotlin:
        ok &= write_or_check(render_kotlin(schema), args.out_kotlin, args.check)

    if not (args.out_cpp or args.out_swift or args.out_kotlin):
        parser.error("at least one of --out-cpp, --out-swift, --out-kotlin is required")

    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
