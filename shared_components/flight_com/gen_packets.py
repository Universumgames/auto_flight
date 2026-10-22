#!/usr/bin/env python3
"""Generates the flight_com packet definitions from packets.jtd.json.

packets.jtd.json is an RFC 8927 JSON Type Definition plus code generation hints in
`metadata`, with one extension: a definition with metadata.extends inherits the
properties of that definition, so packets don't repeat the BasePacket fields.
--out-jtd writes the flattened plain-JTD equivalent for standard JTD validators.
The official jtd-codegen has no C++ or Kotlin target, hence this script. It emits:

  - C++, as two headers:
      FlightTypes.generated.hpp    constants, enum class PacketType, the shared enums
                                   (NLOHMANN_JSON_SERIALIZE_ENUM) and structs
                                   (NLOHMANN_DEFINE_TYPE_INTRUSIVE); only needs nlohmann
      FlightPackets.generated.hpp  BasePacket and every packet struct
                                   (NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE)
  - Kotlin, as two files (same split as the hand-written FrontendPackets.kt /
    FrontendPacketsWire.kt, see the Swift Export note there):
      FrontendPackets.generated.kt      public plain types + FrontendPackets CBOR codec,
                                        the API Swift and Android use
      FrontendPacketsWire.generated.kt  `internal` @Serializable wire twins + conversions

Usage:
    python3 gen_packets.py --out-cpp <dir>      # writes both C++ headers into <dir>
    python3 gen_packets.py --out-jtd <file>     # flattened plain JTD
    python3 gen_packets.py --out-kotlin <dir>   # writes both Kotlin files into <dir>
    # exits 1 if the given files differ from what would be generated:
    python3 gen_packets.py --check --out-cpp <dir> --out-kotlin <dir>

Metadata keys understood (all optional, "//"-prefixed keys are comments):
  root:        constants{NAME: {type, value, description}}, packetTypes{NAME: {value,
               packet, description, group}}, cpp.packetIncludes[], kotlin.package,
               kotlin.imports[], kotlin.prelude[]
  definition:  description, basePacket, extends, cppBody[] (raw lines inside the struct),
               values{wire: int} + descriptions{wire: text} + kotlinExtraValues[] (enums, Kotlin-only values placed first),
               kotlinName, kotlinExternal (use an existing Kotlin class, only generate the
               wire twin), kotlinRejectNaN, kotlinDecoder, kotlinAliases{alias: field or field.path}
  property /
  type alias:  description, default (JSON value), cppDefault (C++ expression), cppType,
               kotlinType, enum: "PacketType" (uint8 holding a PacketType)
"""
import argparse
import json
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_SCHEMA = SCRIPT_DIR / "packets.jtd.json"

GENERATED_NOTICE = (
    "GENERATED FILE - DO NOT EDIT BY HAND.\n"
    "Source of truth: shared_components/flight_com/packets.jtd.json\n"
    "Regenerate with shared_components/flight_com/gen_packets.py"
)

CPP_TYPES = {
    "boolean": "bool", "float32": "float", "float64": "double",
    "int8": "int8_t", "uint8": "uint8_t", "int16": "int16_t", "uint16": "uint16_t",
    "int32": "int32_t", "uint32": "uint32_t", "string": "std::string", "timestamp": "std::string",
}
KOTLIN_TYPES = {
    "boolean": "Boolean", "float32": "Float", "float64": "Double",
    "int8": "Byte", "uint8": "UByte", "int16": "Short", "uint16": "UShort",
    "int32": "Int", "uint32": "UInt", "string": "String", "timestamp": "String",
}


def strip_comments(value):
    if isinstance(value, dict):
        return {k: strip_comments(v) for k, v in value.items() if not k.startswith("//")}
    if isinstance(value, list):
        return [strip_comments(v) for v in value]
    return value


def parse_int(value) -> int:
    return value if isinstance(value, int) else int(value, 0)


def upper(wire: str) -> str:
    """connecting -> CONNECTING, someValue -> SOME_VALUE"""
    out = "".join("_" + c if c.isupper() else c for c in wire)
    return out.replace("-", "_").upper()


def doc_lines(text: str | None) -> list[str]:
    return text.splitlines() if text else []


# --------------------------------------------------------------------------------------
# Schema model
# --------------------------------------------------------------------------------------

def flatten_jtd(raw: dict) -> dict:
    """The schema as plain RFC 8927 JTD: inherited properties copied into every extending definition."""
    flat = json.loads(json.dumps(raw))
    defs = flat.get("definitions", {})
    for d in defs.values():
        parent = d.get("metadata", {}).get("extends")
        if parent:
            d["properties"] = {**defs[parent]["properties"], **d.get("properties", {})}
    return flat


class Field:
    """One property, with refs to type aliases (e.g. Timestamp) already resolved."""

    def __init__(self, name: str, schema: dict, defs: dict):
        self.name = name
        self.meta = dict(schema.get("metadata", {}))
        self.elements = "elements" in schema
        if self.elements:
            schema = schema["elements"]
            self.meta = {**schema.get("metadata", {}), **self.meta}
        if "values" in schema or schema.get("nullable"):
            raise SystemExit(f"error: {name}: `values` and `nullable` are not supported")
        self.ref = schema.get("ref")
        self.prim = schema.get("type")
        # Inline type aliases: a ref to a definition in "type" form carries its metadata along.
        if self.ref and "type" in defs[self.ref]:
            alias = defs[self.ref]
            alias_meta = {k: v for k, v in alias.get("metadata", {}).items() if k != "description"}
            self.meta = {**alias_meta, **self.meta}
            self.prim, self.ref = alias["type"], None
        if self.ref is None and self.prim is None:
            raise SystemExit(f"error: {name}: needs a `type`, `ref` or `elements`")

    @property
    def is_packet_type(self) -> bool:
        return self.meta.get("enum") == "PacketType"


class Schema:
    def __init__(self, raw: dict):
        raw = strip_comments(raw)
        self.meta = raw.get("metadata", {})
        self.defs = raw.get("definitions", {})
        self.constants = self.meta.get("constants", {})
        self.packet_types = {name: {**e, "value": parse_int(e["value"])}
                             for name, e in self.meta.get("packetTypes", {}).items()}

        self.enums, self.structs, self.packets = {}, {}, {}
        bases = [n for n, d in self.defs.items() if d.get("metadata", {}).get("basePacket")]
        if len(bases) != 1:
            raise SystemExit("error: exactly one definition needs metadata.basePacket")
        base_def = self.defs[bases[0]]
        self.base = (bases[0], base_def, [Field(n, p, self.defs) for n, p in base_def["properties"].items()])

        for name, d in self.defs.items():
            m = d.get("metadata", {})
            if "enum" in d:
                self.enums[name] = d
            elif m.get("basePacket"):
                continue
            elif "properties" in d:
                fields = [Field(n, p, self.defs) for n, p in d["properties"].items()]
                if m.get("extends"):
                    # JTD has no inheritance: the base properties are merged in here (and by
                    # flatten_jtd() for plain JTD validators), the definition only lists its own.
                    self.packets[name] = (d, self.base[2] + fields)
                else:
                    self.structs[name] = (d, fields)
            elif "type" not in d:
                raise SystemExit(f"error: definition {name}: unsupported form")
        self.validate()

    def validate(self):
        base_name, _, base_fields = self.base
        base_names = [f.name for f in base_fields]
        for name, (d, fields) in self.packets.items():
            if d["metadata"]["extends"] != base_name:
                raise SystemExit(f"error: {name} extends unknown {d['metadata']['extends']}")
            redeclared = set(base_names) & set(d.get("properties", {}))
            if redeclared:
                raise SystemExit(f"error: {name} redeclares inherited {base_name} properties {sorted(redeclared)}")
        for name, entry in self.packet_types.items():
            if entry.get("packet") and entry["packet"] not in self.packets:
                raise SystemExit(f"error: packetTypes.{name}: unknown packet {entry['packet']}")
        sent = [e["packet"] for e in self.packet_types.values() if e.get("packet")]
        for name in self.packets:
            if sent.count(name) != 1:
                raise SystemExit(f"error: packet {name} must appear exactly once in packetTypes")
        for name, d in self.enums.items():
            values = d.get("metadata", {}).get("values", {})
            if set(values) - set(d["enum"]):
                raise SystemExit(f"error: enum {name}: `values` has entries not in `enum`")

    def own_fields(self, fields: list[Field]) -> list[Field]:
        return fields[len(self.base[2]):]

    def packet_type_of(self, packet: str) -> str:
        return next(n for n, e in self.packet_types.items() if e.get("packet") == packet)

    def struct_order(self) -> list[str]:
        """Shared structs, dependencies first."""
        order, seen = [], set()

        def visit(name):
            if name in seen or name not in self.structs:
                return
            seen.add(name)
            for f in self.structs[name][1]:
                if f.ref:
                    visit(f.ref)
            order.append(name)

        for name in self.structs:
            visit(name)
        return order


# --------------------------------------------------------------------------------------
# C++
# --------------------------------------------------------------------------------------

def cpp_type(s: Schema, f: Field) -> str:
    if "cppType" in f.meta:
        t = f.meta["cppType"]
    elif f.is_packet_type:
        t = "PacketType"
    elif f.ref:
        t = f.ref
    else:
        t = CPP_TYPES[f.prim]
    return f"std::vector<{t}>" if f.elements else t


def cpp_literal(s: Schema, f: Field, value) -> str:
    if f.is_packet_type:
        return f"PacketType::{value}"
    if f.ref in s.enums:
        return f"{f.ref}::{upper(value)}"
    if f.ref in s.structs:
        fields = s.structs[f.ref][1]
        inits = ", ".join(f".{sub.name} = {cpp_literal(s, sub, value[sub.name])}" for sub in fields if sub.name in value)
        return f"{f.ref}{{{inits}}}"
    if isinstance(value, bool):
        return "true" if value else "false"
    if f.prim in ("float32",):
        return f"{float(value)}f"
    if f.prim in ("float64",):
        return f"{float(value)}"
    if f.prim in ("string", "timestamp"):
        return json.dumps(value)
    return str(value)


def cpp_default(s: Schema, f: Field) -> str:
    """Initializer expression for a member / parameter default."""
    if "cppDefault" in f.meta:
        return f.meta["cppDefault"]
    if "default" in f.meta and not f.elements:
        return cpp_literal(s, f, f.meta["default"])
    t = cpp_type(s, f)
    if f.elements or f.ref in s.structs:
        return f"{t}{{}}"
    if f.is_packet_type:
        return f"PacketType::{next(iter(s.packet_types))}"
    if f.ref in s.enums:
        return f"{f.ref}::{upper(s.enums[f.ref]['enum'][0])}"
    if f.prim == "boolean":
        return "false"
    if f.prim in ("string", "timestamp"):
        return '""'
    return "0"


def cpp_doc(text, indent="") -> list[str]:
    return [f"{indent}/// {line}" for line in doc_lines(text)]


def cpp_members(s: Schema, fields: list[Field]) -> list[str]:
    out = []
    for f in fields:
        out += cpp_doc(f.meta.get("description"), "    ")
        out.append(f"    {cpp_type(s, f)} {f.name} = {cpp_default(s, f)};")
    return out


def cpp_to_string(name: str, fields: list[Field], base: str | None = None) -> str:
    parts = [f'std::string("{name}{{")']
    sep = ""
    if base:
        parts.append(f'"base=" + {base}::toString()')
        sep = ", "
    for f in fields:
        parts.append(f'"{sep}{f.name}=" + flight_com_detail::toString({f.name})')
        sep = ", "
    parts.append('"}"')
    return " + ".join(parts)


CPP_TYPES_FILE = "FlightTypes.generated.hpp"
CPP_PACKETS_FILE = "FlightPackets.generated.hpp"


def gen_cpp(s: Schema) -> dict[str, str]:
    """Returns {file name: content}: the plain types header (only needs nlohmann) and the
    packets header on top of it (also needs metadata.cpp.packetIncludes, e.g. DeviceId)."""
    base_name, base_def, base_fields = s.base
    notice = [f"// {line}" for line in GENERATED_NOTICE.splitlines()]
    out = ["#pragma once", ""] + notice + [""]
    out += [f"#include <{inc}>" for inc in ("cstdint", "cstddef", "string", "type_traits", "vector")]
    out += ["", "#include <nlohmann/json.hpp>", ""]

    # constants
    out.append("// ---- constants ----")
    for name, c in s.constants.items():
        out += cpp_doc(c.get("description"))
        out.append(f"inline constexpr {CPP_TYPES[c['type']]} {name} = {c['value']};")
    out.append("")

    # PacketType
    out.append("enum class PacketType: uint8_t {")
    group = None
    for name, e in s.packet_types.items():
        if e.get("group") and e["group"] != group:
            group = e["group"]
            out.append(f"    // {group}")
        out += cpp_doc(e.get("description"), "    ")
        out.append(f"    {name} = 0x{e['value']:02X},")
    out.append("};")
    out.append("")
    out.append("constexpr const char* packetTypeName(PacketType type) {")
    out.append("    switch (type) {")
    for name in s.packet_types:
        out.append(f'    case PacketType::{name}: return "{name}";')
    out.append("    }")
    out.append('    return "UNKNOWN";')
    out.append("}")
    out.append("")
    out.append("constexpr bool isKnownPacketType(uint8_t value) {")
    out.append("    switch (value) {")
    for e in s.packet_types.values():
        out.append(f"    case 0x{e['value']:02X}:")
    out.append("        return true;")
    out.append("    default:")
    out.append("        return false;")
    out.append("    }")
    out.append("}")
    out.append("")

    # enums
    for name, d in s.enums.items():
        m = d.get("metadata", {})
        values = {k: parse_int(v) for k, v in m.get("values", {}).items()}
        descs = m.get("descriptions", {})
        out += cpp_doc(m.get("description"))
        out.append(f"enum class {name}: uint8_t {{")
        for i, wire in enumerate(d["enum"]):
            value = values.get(wire, i)
            comment = f" ///< {descs[wire]}" if wire in descs else ""
            out.append(f"    {upper(wire)} = 0x{value:02X},{comment}")
        out.append("};")
        out.append("")
        out.append(f"NLOHMANN_JSON_SERIALIZE_ENUM({name}, {{")
        for wire in d["enum"]:
            out.append(f'    {{{name}::{upper(wire)}, "{wire}"}},')
        out.append("})")
        out.append("")

    # toString helper used by every generated struct
    out += [
        "namespace flight_com_detail {",
        "template <typename T>",
        "std::string toString(const T& value) {",
        "    if constexpr (std::is_same_v<T, bool>) return value ? \"true\" : \"false\";",
        "    else if constexpr (std::is_enum_v<T>) return std::to_string(static_cast<int>(value));",
        "    else if constexpr (std::is_arithmetic_v<T>) return std::to_string(value);",
        "    else if constexpr (std::is_same_v<T, std::string>) return value;",
        "    else return value.toString();",
        "}",
        "",
        "template <typename T>",
        "std::string toString(const std::vector<T>& value) {",
        "    return \"[\" + std::to_string(value.size()) + \" items]\";",
        "}",
        "} // namespace flight_com_detail",
        "",
    ]

    # shared structs
    for name in s.struct_order():
        d, fields = s.structs[name]
        m = d.get("metadata", {})
        out += cpp_doc(m.get("description"))
        out.append(f"struct {name} {{")
        out += cpp_members(s, fields)
        out.append("")
        out.append(f"    NLOHMANN_DEFINE_TYPE_INTRUSIVE({name}, {', '.join(f.name for f in fields)})")
        out.append("")
        out.append(f"    bool operator==(const {name}& other) const = default;")
        out.append("")
        out.append("    std::string toString() const {")
        out.append(f"        return {cpp_to_string(name, fields)};")
        out.append("    }")
        if m.get("cppBody"):
            out.append("")
            out += [f"    {line}" if line else "" for line in m["cppBody"]]
        out.append("};")
        out.append("")

    types_header = "\n".join(out)

    # packet infrastructure
    out = ["#pragma once", ""] + notice + [""]
    out += [f"#include <{inc}>" for inc in ("cstring", "ctime", "memory", "utility")]
    out += ["", f'#include "{CPP_TYPES_FILE}"']
    out += [f"#include {inc}" for inc in s.meta.get("cpp", {}).get("packetIncludes", [])]
    out.append("")
    out += [
        "typedef const uint8_t* RawSerializedPacket;",
        "typedef std::pair<std::unique_ptr<uint8_t[]>, size_t> SerializedPacket;",
        "",
        "#define SERIALIZE_THIS_PACKET() \\",
        "    nlohmann::json j = *this; \\",
        "    std::vector<uint8_t> buffer; \\",
        "    try{ \\",
        "        nlohmann::json::to_cbor(j, buffer); \\",
        "    } catch (const std::exception& e) { \\",
        "        ESP_LOGE(\"BasePacket\", \"Error serializing packet: %s\", e.what()); \\",
        "    } \\",
        "    auto packet = std::make_unique<uint8_t[]>(buffer.size()); \\",
        "    std::memcpy(packet.get(), buffer.data(), buffer.size()); \\",
        "    return {std::move(packet), buffer.size()};",
        "",
        "#define DESERIALIZE_TO_THIS_PACKET(packet, len) \\",
        "    try { \\",
        "        nlohmann::json j = nlohmann::json::from_cbor(packet, packet + len); \\",
        "        j.get_to(*this); \\",
        "    } catch (const std::exception& e) { \\",
        "        ESP_LOGE(\"BasePacket\", \"Error deserializing packet: %s\", e.what()); \\",
        "    }",
        "",
        "struct IBasePacket {",
        "    virtual std::string toString() = 0;",
        "    virtual ~IBasePacket() = default;",
        "    [[nodiscard]] virtual SerializedPacket serialize() const = 0;",
        "};",
        "",
    ]

    # BasePacket
    ts, ty, did = base_fields[:3]
    out += cpp_doc(base_def.get("metadata", {}).get("description"))
    out.append(f"struct {base_name} : public IBasePacket {{")
    out += cpp_members(s, base_fields)
    out += [
        "",
        "    std::string toString() override {",
        f"        return {cpp_to_string(base_name, base_fields)};",
        "    }",
        "",
        f"    {base_name}(const {base_name}& packet) = default;",
        "",
        f"    {base_name}(RawSerializedPacket packet, size_t len) {{",
        "        DESERIALIZE_TO_THIS_PACKET(packet, len);",
        "    }",
        "",
        f"    {base_name}({cpp_type(s, ts)} time = 0, PacketType type = {cpp_default(s, ty)}) : {ts.name}(time), {ty.name}(type) {{}}",
        "",
        f"    NLOHMANN_DEFINE_TYPE_INTRUSIVE({base_name}, {', '.join(f.name for f in base_fields)})",
        "",
        "    [[nodiscard]] SerializedPacket serialize() const override {",
        "        SERIALIZE_THIS_PACKET();",
        "    }",
        "",
        "private:",
        f"    {base_name}({cpp_type(s, ts)} time, PacketType type, {cpp_type(s, did)} id) : {ts.name}(time), {ty.name}(type), {did.name}(id) {{}}",
        "public:",
        f"    static {base_name} nullPacket({cpp_type(s, did)} id, PacketType type, {cpp_type(s, ts)} time = 0) {{",
        "        return {time, type, id};",
        "    }",
        "};",
        "",
    ]

    # packets
    for name, (d, fields) in s.packets.items():
        m = d.get("metadata", {})
        own = s.own_fields(fields)
        ptype = s.packet_type_of(name)
        params = ", ".join([f"{cpp_type(s, ts)} time = 0"] + [f"{cpp_type(s, f)} {f.name} = {cpp_default(s, f)}" for f in own])
        inits = ", ".join([f"{base_name}(time, TYPE)"] + [
            f"{f.name}(std::move({f.name}))" if f.elements or f.ref in s.structs else f"{f.name}({f.name})" for f in own])
        out += cpp_doc(m.get("description"))
        out.append(f"struct {name} : public {base_name} {{")
        out.append(f"    static constexpr PacketType TYPE = PacketType::{ptype};")
        out.append("")
        out += cpp_members(s, own)
        out += [
            "",
            "    std::string toString() override {",
            f"        return {cpp_to_string(name, own, base_name)};",
            "    }",
            "",
            f"    {name}(const {name}& packet) = default;",
            "",
            f"    {name}(RawSerializedPacket packet, size_t len) : {base_name}() {{",
            "        DESERIALIZE_TO_THIS_PACKET(packet, len);",
            "    }",
            "",
            f"    {name}({params}) :",
            f"        {inits} {{}}",
            "",
            "    [[nodiscard]] SerializedPacket serialize() const override {",
            "        SERIALIZE_THIS_PACKET();",
            "    }",
            "",
            f"    NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE({name}, {base_name}, {', '.join(f.name for f in own)})",
        ]
        if m.get("cppBody"):
            out.append("")
            out += [f"    {line}" if line else "" for line in m["cppBody"]]
        out.append("};")
        out.append("")
    return {CPP_TYPES_FILE: types_header, CPP_PACKETS_FILE: "\n".join(out)}


# --------------------------------------------------------------------------------------
# Kotlin
# --------------------------------------------------------------------------------------

def kt_name(s: Schema, name: str) -> str:
    """Public Kotlin name of a definition."""
    d = s.defs[name]
    m = d.get("metadata", {})
    return m.get("kotlinExternal") or m.get("kotlinName") or name


def kt_wire_name(name: str) -> str:
    return f"{name}Wire"


def kt_scalar(s: Schema, f: Field, wire: bool) -> str:
    if f.is_packet_type:
        return "Int" if wire else "PacketType"
    if f.ref:
        return kt_wire_name(f.ref) if wire else kt_name(s, f.ref)
    return f.meta.get("kotlinType") or KOTLIN_TYPES[f.prim]


def kt_type(s: Schema, f: Field, wire: bool) -> str:
    t = kt_scalar(s, f, wire)
    return f"List<{t}>" if f.elements else t


def kt_needs_conversion(f: Field) -> bool:
    return f.is_packet_type or f.ref is not None


def kt_convert(s: Schema, f: Field, expr: str, to_wire: bool) -> str:
    if not kt_needs_conversion(f):
        return expr
    if f.is_packet_type:
        one = (lambda e: f"{e}.wireValue") if to_wire else (lambda e: f"packetTypeOf({e})")
    else:
        one = (lambda e: f"{e}.toWire()") if to_wire else (lambda e: f"{e}.toPublic()")
    return f"{expr}.map {{ {one('it')} }}" if f.elements else one(expr)


def kt_literal(s: Schema, f: Field, value) -> str:
    if f.is_packet_type:
        return f"PacketType.{value}"
    if f.ref in s.enums:
        return f"{kt_name(s, f.ref)}.{upper(value)}"
    if f.ref in s.structs:
        fields = s.structs[f.ref][1]
        args = ", ".join(f"{sub.name} = {kt_literal(s, sub, value[sub.name])}" for sub in fields if sub.name in value)
        return f"{kt_name(s, f.ref)}({args})"
    t = kt_scalar(s, f, wire=False)
    if isinstance(value, bool):
        return "true" if value else "false"
    if t == "Float":
        return f"{float(value)}f"
    if t == "Double":
        return f"{float(value)}"
    if t in ("UInt", "UByte", "UShort"):
        return f"{value}u"
    if t == "ULong":
        return f"{value}uL"
    if t == "Long":
        return f"{value}L"
    if t == "String":
        return json.dumps(value)
    return str(value)


def kt_doc(text, indent="") -> list[str]:
    lines = doc_lines(text)
    if not lines:
        return []
    if len(lines) == 1:
        return [f"{indent}/** {lines[0]} */"]
    return [f"{indent}/**"] + [f"{indent} * {line}" for line in lines] + [f"{indent} */"]


def kt_param(s: Schema, f: Field, override: bool = False) -> list[str]:
    out = kt_doc(f.meta.get("description"), "    ")
    default = f" = {kt_literal(s, f, f.meta['default'])}" if "default" in f.meta and not f.elements else ""
    out.append(f"    {'override ' if override else ''}val {f.name}: {kt_type(s, f, wire=False)}{default},")
    return out


def kt_wire_class(s: Schema, name: str, fields: list[Field]) -> list[str]:
    # No default values here: Cbor would skip encoding a field equal to its default, which
    # the C++ side (NLOHMANN_DEFINE_TYPE_INTRUSIVE requires every key) rejects.
    out = ["@Serializable", f"internal data class {kt_wire_name(name)}("]
    out += [f"    val {f.name}: {kt_type(s, f, wire=True)}," for f in fields]
    out.append(")")
    return out


KOTLIN_PUBLIC_FILE = "FrontendPackets.generated.kt"
KOTLIN_WIRE_FILE = "FrontendPacketsWire.generated.kt"


def kt_header(km: dict, imports: list[str]) -> list[str]:
    out = [f"// {line}" for line in GENERATED_NOTICE.splitlines()]
    out += ["", f"package {km['package']}", ""]
    out += [f"import {i}" for i in imports]
    out.append("")
    return out


def gen_kotlin(s: Schema) -> dict[str, str]:
    """Returns {file name: content}: the public (Swift-facing) file and the internal wire file."""
    km = s.meta.get("kotlin", {})
    base_name, base_def, base_fields = s.base
    ts, ty, did = base_fields[:3]

    pub = kt_header(km, list(km.get("imports", [])) + [
        "kotlinx.serialization.ExperimentalSerializationApi",
        "kotlinx.serialization.cbor.Cbor",
        "kotlinx.serialization.decodeFromByteArray",
        "kotlinx.serialization.encodeToByteArray",
        "kotlin.time.Clock",
    ])
    pub += [
        "/*",
        " * Public packet types, the API Swift (via Swift Export) and Android use. They are plain data",
        " * holders, not @Serializable: a public @Serializable class drags kotlinx-serialization-core",
        " * into Swift Export, which trips a Swift Export Alpha bug. The CBOR codec [FrontendPackets]",
        f" * below works on the `internal` *Wire twins in {KOTLIN_WIRE_FILE} instead.",
        " */",
        "",
    ]
    wire = kt_header(km, list(km.get("imports", [])) + [
        "kotlinx.serialization.SerialName",
        "kotlinx.serialization.Serializable",
    ])
    wire += [
        "/*",
        f" * `internal` @Serializable twins of the public types in {KOTLIN_PUBLIC_FILE}, plus the",
        " * conversions to/from them. Swift Export only bridges `public` declarations, so keeping",
        " * these `internal` keeps kotlinx-serialization out of its view.",
        " *",
        " * Every packet repeats the BasePacket fields, since NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE",
        " * flattens them into the same CBOR map. No wire class has default values: Cbor would skip",
        " * encoding a field equal to its default, which the C++ side (every key required) rejects.",
        " */",
        "",
        "private fun packetTypeOf(wireValue: Int) =",
        '    requireNotNull(PacketType.fromWireValue(wireValue)) { "unknown packet type $wireValue" }',
        "",
        "private fun Int.requireType(expected: PacketType) =",
        '    require(this == expected.wireValue) { "expected packet type ${expected.wireValue}, got $this" }',
        "",
    ]

    if km.get("prelude"):
        pub += km["prelude"] + [""]

    # constants
    pub.append("object FlightComConstants {")
    for name, c in s.constants.items():
        pub += kt_doc(c.get("description"), "    ")
        f = Field(name, {"type": c["type"]}, s.defs)
        pub.append(f"    const val {name}: {kt_scalar(s, f, False)} = {kt_literal(s, f, c['value'])}")
    pub += ["}", ""]

    # PacketType
    pub.append("/** Mirrors `PacketType`; on the wire it's the plain `uint8_t` [wireValue]. */")
    pub.append("enum class PacketType(val wireValue: Int) {")
    group = None
    entries = list(s.packet_types.items())
    for i, (name, e) in enumerate(entries):
        if e.get("group") and e["group"] != group:
            group = e["group"]
            pub.append(f"    // {group}")
        pub += kt_doc(e.get("description"), "    ")
        pub.append(f"    {name}(0x{e['value']:02X}){';' if i == len(entries) - 1 else ','}")
    pub += [
        "",
        "    companion object {",
        "        fun fromWireValue(value: Int): PacketType? = entries.firstOrNull { it.wireValue == value }",
        "    }",
        "}",
        "",
    ]

    # enums
    for name, d in s.enums.items():
        m = d.get("metadata", {})
        wires = m.get("kotlinExtraValues", []) + d["enum"]
        descs = m.get("descriptions", {})
        pname, wname = kt_name(s, name), kt_wire_name(name)
        pub += kt_doc(m.get("description") or f"Mirrors `{name}`; wire values are the NLOHMANN_JSON_SERIALIZE_ENUM strings.")
        pub.append(f"enum class {pname} {{")
        for w in wires:
            pub += kt_doc(descs.get(w), "    ")
            pub.append(f"    {upper(w)},")
        pub += ["}", ""]

        wire += ["@Serializable", f"internal enum class {wname} {{"]
        wire += [f'    @SerialName("{w}") {upper(w)},' for w in wires]
        wire += ["}", ""]
        wire.append(f"internal fun {pname}.toWire() = when (this) {{")
        wire += [f"    {pname}.{upper(w)} -> {wname}.{upper(w)}" for w in wires]
        wire += ["}", ""]
        wire.append(f"internal fun {wname}.toPublic() = when (this) {{")
        wire += [f"    {wname}.{upper(w)} -> {pname}.{upper(w)}" for w in wires]
        wire += ["}", ""]

    # shared structs
    for name in s.struct_order():
        d, fields = s.structs[name]
        m = d.get("metadata", {})
        pname, wname = kt_name(s, name), kt_wire_name(name)
        if not m.get("kotlinExternal"):
            pub += kt_doc(m.get("description") or f"Mirrors `{name}`.")
            pub.append(f"data class {pname}(")
            for f in fields:
                pub += kt_param(s, f)
            pub += [")", ""]
        wire += kt_wire_class(s, name, fields)
        wire.append("")
        args = ", ".join(f"{f.name} = {kt_convert(s, f, f.name, True)}" for f in fields)
        wire.append(f"internal fun {pname}.toWire() = {wname}({args})")
        wire.append("")
        args = ", ".join(f"{f.name} = {kt_convert(s, f, f.name, False)}" for f in fields)
        if m.get("kotlinRejectNaN"):
            floats = [f.name for f in fields if kt_scalar(s, f, True) in ("Float", "Double") and not f.elements]
            cond = " && ".join(f"!{n}.isNaN()" for n in floats)
            wire += [
                "/** Throws on a NaN component, so the enclosing packet decodes to `null` as a whole. */",
                f"internal fun {wname}.toPublic(): {pname} {{",
                f'    require({cond}) {{ "{name} has NaN component: $this" }}',
                f"    return {pname}({args})",
                "}",
            ]
        else:
            wire.append(f"internal fun {wname}.toPublic() = {pname}({args})")
        wire.append("")

    # packet base
    pub += kt_doc("Common supertype of all packets: the fields every packet inherits from `BasePacket`.")
    pub.append("sealed interface FrontendPacket {")
    for f in base_fields:
        pub.append(f"    val {f.name}: {kt_type(s, f, wire=False)}")
    pub += [
        "",
        f"    /** Alias for [{did.name}]: the device id of the packet's sender. */",
        f"    val sourceId: {kt_type(s, did, False)} get() = {did.name}",
        "}",
        "",
        f"/** Current time as `time_t`-compatible epoch seconds, the default [FrontendPacket.{ts.name}]. */",
        "fun currentTimestamp(): Long = Clock.System.now().epochSeconds",
        "",
    ]
    pub += kt_doc(base_def.get("metadata", {}).get("description"))
    pub += [
        f"data class {base_name}(",
        f"    override val {ty.name}: PacketType,",
        f"    override val {did.name}: {kt_type(s, did, False)},",
        f"    override val {ts.name}: {kt_type(s, ts, False)} = currentTimestamp(),",
        ") : FrontendPacket",
        "",
    ]
    wire += kt_wire_class(s, base_name, base_fields)
    wire += [
        "",
        f"internal fun {base_name}.toWire() = {kt_wire_name(base_name)}({ts.name}, {ty.name}.wireValue, {did.name})",
        f"internal fun {kt_wire_name(base_name)}.toPublic() = {base_name}({ty.name} = packetTypeOf({ty.name}), {did.name} = {did.name}, {ts.name} = {ts.name})",
        "",
    ]

    # packets
    for name, (d, fields) in s.packets.items():
        m = d.get("metadata", {})
        own = s.own_fields(fields)
        pname, wname = kt_name(s, name), kt_wire_name(name)
        ptype = s.packet_type_of(name)
        pub += kt_doc(m.get("description") or f"Mirrors `{name}` ({ptype}).")
        pub.append(f"data class {pname}(")
        pub.append(f"    override val {did.name}: {kt_type(s, did, False)},")
        for f in own:
            pub += kt_param(s, f)
        pub.append(f"    override val {ts.name}: {kt_type(s, ts, False)} = currentTimestamp(),")
        pub.append(") : FrontendPacket {")
        pub.append(f"    override val {ty.name} get() = PacketType.{ptype}")
        for alias, target in m.get("kotlinAliases", {}).items():
            # target is a field path, e.g. "metadata.hash" for a field of a nested struct
            candidates, target_field = own, None
            for i, part in enumerate(target.split(".")):
                target_field = next((f for f in candidates if f.name == part), None)
                if target_field is None or (i < target.count(".") and (target_field.elements or not target_field.ref)):
                    raise SystemExit(f"error: {name}: kotlinAliases.{alias}: unknown field path {target!r}")
                if target_field.ref:
                    candidates = [Field(n, p, s.defs) for n, p in s.defs[target_field.ref].get("properties", {}).items()]
            pub.append(f"    val {alias}: {kt_type(s, target_field, False)} get() = {target}")
        pub += ["}", ""]

        wire += kt_wire_class(s, name, fields)
        wire.append("")
        wire.append(f"internal fun {pname}.toWire() = {wname}(")
        wire.append(f"    {ts.name} = {ts.name},")
        wire.append(f"    {ty.name} = {ty.name}.wireValue,")
        wire.append(f"    {did.name} = {did.name},")
        wire += [f"    {f.name} = {kt_convert(s, f, f.name, True)}," for f in own]
        wire += [")", ""]
        wire.append(f"internal fun {wname}.toPublic(): {pname} {{")
        wire.append(f"    {ty.name}.requireType(PacketType.{ptype})")
        wire.append(f"    return {pname}(")
        wire.append(f"        {ts.name} = {ts.name},")
        wire.append(f"        {did.name} = {did.name},")
        wire += [f"        {f.name} = {kt_convert(s, f, f.name, False)}," for f in own]
        wire += ["    )", "}", ""]

    # codec (public API, but implemented on the internal wire types)
    def decoder(name):
        return s.defs[name].get("metadata", {}).get("kotlinDecoder") or f"decode{name}"

    pub += [
        "/**",
        " * CBOR codec for the packets above. Bytes cross the Kotlin/Swift boundary as hex strings,",
        " * since Swift Export (Alpha) can't construct a Kotlin `ByteArray` from Swift.",
        " */",
        "@OptIn(ExperimentalSerializationApi::class, ExperimentalStdlibApi::class)",
        "object FrontendPackets {",
        "    private val cbor = Cbor {",
        "        ignoreUnknownKeys = true",
        "        encodeDefaults = true",
        "    }",
        "",
        "    /** Decodes any packet, dispatching on its wire `type`; `null` if malformed or of unknown type. */",
        "    fun decode(hex: String): FrontendPacket? {",
        "        val base = decodeBase(hex) ?: return null",
        f"        return when (base.{ty.name}) {{",
    ]
    for ptname, e in s.packet_types.items():
        target = f"{decoder(e['packet'])}(hex)" if e.get("packet") else "base"
        pub.append(f"            PacketType.{ptname} -> {target}")
    pub += [
        "        }",
        "    }",
        "",
        f"    fun decodeBase(hex: String): {base_name}? =",
        f"        runCatching {{ cbor.decodeFromByteArray<{kt_wire_name(base_name)}>(hex.hexToByteArray()).toPublic() }}.getOrNull()",
    ]
    for name in s.packets:
        pub += [
            "",
            f"    fun {decoder(name)}(hex: String): {kt_name(s, name)}? =",
            f"        runCatching {{ cbor.decodeFromByteArray<{kt_wire_name(name)}>(hex.hexToByteArray()).toPublic() }}.getOrNull()",
        ]
    pub += [
        "",
        "    /** Encodes any [FrontendPacket] to its CBOR wire form, hex-encoded. */",
        "    fun encode(packet: FrontendPacket): String = when (packet) {",
        f"        is {base_name} -> cbor.encodeToByteArray(packet.toWire())",
    ]
    pub += [f"        is {kt_name(s, name)} -> cbor.encodeToByteArray(packet.toWire())" for name in s.packets]
    pub += ["    }.toHexString()", "}", ""]
    return {KOTLIN_PUBLIC_FILE: "\n".join(pub), KOTLIN_WIRE_FILE: "\n".join(wire)}


# --------------------------------------------------------------------------------------

def emit(path: Path, content: str, check: bool) -> bool:
    if check:
        if not path.exists() or path.read_text(encoding="utf-8") != content:
            print(f"out of date: {path}", file=sys.stderr)
            return False
        return True
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_text(encoding="utf-8") != content:
        path.write_text(content, encoding="utf-8")  # keep mtime stable when unchanged
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--schema", type=Path, default=DEFAULT_SCHEMA)
    ap.add_argument("--out-cpp", type=Path, metavar="DIR",
                    help=f"directory to write {CPP_TYPES_FILE} and {CPP_PACKETS_FILE} into")
    ap.add_argument("--out-jtd", type=Path, metavar="FILE",
                    help="write the schema flattened to plain RFC 8927 JTD (inherited properties expanded)")
    ap.add_argument("--out-kotlin", type=Path, metavar="DIR",
                    help=f"directory to write {KOTLIN_PUBLIC_FILE} and {KOTLIN_WIRE_FILE} into")
    ap.add_argument("--check", action="store_true", help="only verify the outputs are up to date")
    args = ap.parse_args()
    if not (args.out_cpp or args.out_kotlin or args.out_jtd):
        ap.error("nothing to do, pass --out-cpp, --out-kotlin and/or --out-jtd")

    raw = json.loads(args.schema.read_text(encoding="utf-8"))
    schema = Schema(raw)
    ok = True
    if args.out_cpp:
        for file_name, content in gen_cpp(schema).items():
            ok &= emit(args.out_cpp / file_name, content, args.check)
    if args.out_jtd:
        ok &= emit(args.out_jtd, json.dumps(flatten_jtd(raw), indent=2) + "\n", args.check)
    if args.out_kotlin:
        for file_name, content in gen_kotlin(schema).items():
            ok &= emit(args.out_kotlin / file_name, content, args.check)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
