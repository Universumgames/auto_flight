#!/usr/bin/env python3
"""Generates the BLE wire-contract sources from ble_protocol.json.

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
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_SCHEMA = SCRIPT_DIR / "ble_protocol.json"

GENERATED_NOTICE = (
    "GENERATED FILE - DO NOT EDIT BY HAND.\n"
    "Source of truth: base_station/components/frontend_bluetooth/ble_protocol.json\n"
    "Regenerate with base_station/components/frontend_bluetooth/gen_ble_protocol.py"
)


def load_schema(path: Path) -> dict:
    with open(path, "r", encoding="utf-8") as f:
        raw = json.load(f)
    # "//"-prefixed keys are documentation-only comments (JSON has no comment syntax).
    return {k: v for k, v in raw.items() if not k.startswith("//")}


def _int(value) -> int:
    return int(value, 16) if isinstance(value, str) else int(value)


def render_cpp(schema: dict) -> str:
    topics = schema["topics"]
    lines = []
    lines.append("#pragma once")
    lines.append("// " + GENERATED_NOTICE.replace("\n", "\n// "))
    lines.append("//")
    lines.append("// Mirrored on iOS by BluetoothUUIDs.generated.swift and (stub, not yet wired")
    lines.append("// up on Android) by sharedLogic's wire/BleProtocol.generated.kt - keep all")
    lines.append("// three in sync by editing ble_protocol.json and regenerating, never by hand.")
    lines.append("#include <cstddef>")
    lines.append("#include <cstdint>")
    lines.append("")
    lines.append('#include "host/ble_uuid.h"')
    lines.append("")
    lines.append("namespace BLETopics {")
    lines.append("    enum NotifyByte : uint8_t {")
    for t in topics:
        lines.append(f"        {t['name']} = {t['value']},")
    lines.append("    };")
    lines.append("")
    lines.append("    inline constexpr NotifyByte ALL_NOTIFICATIONS[] = {")
    for t in topics:
        lines.append(f"        {t['name']},")
    lines.append("    };")
    lines.append("")
    lines.append("    inline constexpr size_t ALL_NOTIFICATIONS_SIZE = std::size(ALL_NOTIFICATIONS);")
    lines.append("")
    lines.append("    // UUIDs, in the same order as ALL_NOTIFICATIONS (looked up by position, not by enum value).")
    lines.append(f"    inline constexpr ble_uuid16_t TOPIC_UUIDS[ALL_NOTIFICATIONS_SIZE] = {{")
    for t in topics:
        lines.append(f"        BLE_UUID16_INIT(0x{t['uuid']}), // {t['name']}")
    lines.append("    };")
    lines.append("")
    lines.append("    /**")
    lines.append("     * Look up the 16-bit UUID registered for a topic.")
    lines.append("     * @return Pointer to the matching entry in TOPIC_UUIDS, or nullptr if the")
    lines.append("     *         topic has no registered UUID (e.g. an unknown value).")
    lines.append("     */")
    lines.append("    inline const ble_uuid16_t* getUUIDForTopic(const NotifyByte topic) {")
    lines.append("        for (size_t i = 0; i < ALL_NOTIFICATIONS_SIZE; ++i) {")
    lines.append("            if (ALL_NOTIFICATIONS[i] == topic) {")
    lines.append("                return &TOPIC_UUIDS[i];")
    lines.append("            }")
    lines.append("        }")
    lines.append("        return nullptr; // Invalid/unregistered topic")
    lines.append("    }")
    lines.append("}")
    lines.append("")
    lines.append("namespace BLEProtocol {")
    lines.append("    // Default service UUID advertised by this firmware; overridable at build time via")
    lines.append("    // `idf.py menuconfig` -> Bluetooth Handler Configuration (CONFIG_BLUETOOTH_SERVICE_UUID).")
    lines.append("    // Clients that can't read Kconfig (iOS, and eventually Android) compile this default")
    lines.append("    // in directly, so a non-default CONFIG_BLUETOOTH_SERVICE_UUID requires updating")
    lines.append("    // ble_protocol.json too.")
    lines.append(f"    inline constexpr uint16_t SERVICE_UUID = 0x{schema['serviceUUID']};")
    lines.append("")
    lines.append("    // Every fragmented notify/write is prefixed with this many bytes: [0:2) totalLength,")
    lines.append("    // [2:4) offset, both little-endian uint16. See BluetoothManager::sendFragmented /")
    lines.append("    // handleFragmentedWrite.")
    lines.append(f"    inline constexpr uint16_t FRAGMENT_HEADER_SIZE = {schema['fragmentHeaderSize']};")
    lines.append("")
    lines.append("    // Build-version manufacturer data advertised in the scan response: company ID (2")
    lines.append("    // bytes LE) + build epoch (N bytes LE). See BluetoothManager::populateBuildVersionMfgData.")
    mfg = schema["manufacturerData"]
    lines.append(f"    inline constexpr uint16_t MFG_COMPANY_ID = 0x{mfg['companyID'].removeprefix('0x').removeprefix('0X')};")
    lines.append(f"    inline constexpr uint16_t MFG_BUILD_EPOCH_FIELD_SIZE = {mfg['buildEpochFieldSize']};")
    lines.append("    inline constexpr uint16_t MFG_DATA_LEN = 2 + MFG_BUILD_EPOCH_FIELD_SIZE;")
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def render_swift(schema: dict) -> str:
    topics = schema["topics"]
    mfg = schema["manufacturerData"]
    lines = []
    lines.append("//")
    lines.append("// " + GENERATED_NOTICE.replace("\n", "\n// "))
    lines.append("//")
    lines.append("// Mirrors base_station/components/frontend_bluetooth/BLETopics.generated.hpp.")
    lines.append("//")
    lines.append("")
    lines.append("import CoreBluetooth")
    lines.append("")
    company_id_hex = mfg["companyID"].removeprefix("0x").removeprefix("0X")
    lines.append(f"let ServiceUUID = CBUUID(string: \"{schema['serviceUUID']}\")")
    lines.append("")
    lines.append("enum BLEProtocolConstants {")
    lines.append(f"    static let fragmentHeaderSize = {schema['fragmentHeaderSize']}")
    lines.append(f"    static let mfgCompanyID: UInt16 = 0x{company_id_hex}")
    lines.append("}")
    lines.append("")
    lines.append("enum NotifyByte: UInt8, CaseIterable, Identifiable {")
    for t in topics:
        lines.append(f"    case {t['name']} = {t['value']}")
    lines.append("")
    lines.append("    var id: UInt8 { rawValue }")
    lines.append("")
    lines.append("    var characteristic: CharacteristicUUID {")
    lines.append("        switch self {")
    for t in topics:
        lines.append(f"            case .{t['name']}: return .{t['swiftCase']}")
    lines.append("        }")
    lines.append("    }")
    lines.append("}")
    lines.append("")
    lines.append("enum CharacteristicUUID: String, CaseIterable, Identifiable {")
    for t in topics:
        lines.append(f"    case {t['swiftCase']} = \"{t['uuid']}\"")
    lines.append("}")
    lines.append("")
    lines.append("extension CharacteristicUUID {")
    lines.append("    var uuid: CBUUID {")
    lines.append("        return CBUUID(string: rawValue)")
    lines.append("    }")
    lines.append("")
    lines.append("    var id: CBUUID {")
    lines.append("        uuid")
    lines.append("    }")
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def render_kotlin(schema: dict) -> str:
    topics = schema["topics"]
    mfg = schema["manufacturerData"]
    company_id_hex = mfg["companyID"].removeprefix("0x").removeprefix("0X")
    lines = []
    lines.append("// " + GENERATED_NOTICE.replace("\n", "\n// "))
    lines.append("//")
    lines.append("// STUB: not referenced by any code yet - the Android app currently talks to the")
    lines.append("// base station over a websocket (see KtorFlightRepository.kt), not Bluetooth.")
    lines.append("// This exists so the day Android/KMP grows a BLE transport, the wire contract is")
    lines.append("// already generated from the same source of truth as the C++ and Swift sides")
    lines.append("// instead of being hand-copied a third time. Regenerate with")
    lines.append("// base_station/components/frontend_bluetooth/gen_ble_protocol.py --out-kotlin <path>")
    lines.append("package de.universegame.auto_flight.app.wire")
    lines.append("")
    lines.append("object BleProtocol {")
    lines.append(f"    const val SERVICE_UUID: String = \"{schema['serviceUUID']}\"")
    lines.append(f"    const val FRAGMENT_HEADER_SIZE: Int = {schema['fragmentHeaderSize']}")
    lines.append(f"    const val MFG_COMPANY_ID: Int = 0x{company_id_hex}")
    lines.append(f"    const val MFG_BUILD_EPOCH_FIELD_SIZE: Int = {mfg['buildEpochFieldSize']}")
    lines.append("    const val MFG_DATA_LEN: Int = 2 + MFG_BUILD_EPOCH_FIELD_SIZE")
    lines.append("}")
    lines.append("")
    lines.append("enum class BleTopic(val value: UByte, val characteristicUUID: String) {")
    for t in topics:
        lines.append(f"    {t['name']}({t['value']}u, \"{t['uuid']}\"),")
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def write_or_check(content: str, out_path: Path, check: bool) -> bool:
    """Returns True if content is/was up to date (i.e. no error)."""
    if check:
        if not out_path.exists() or out_path.read_text(encoding="utf-8") != content:
            print(f"STALE: {out_path} does not match ble_protocol.json - regenerate it", file=sys.stderr)
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
