"""Export a .tflite model and its labels as a C array (<out>.h / <out>.cc) for the vision_controller,
together with a tflite::MicroMutableOpResolver registering exactly the ops the model uses.

Stdlib only, so the vision_controller's CMake build can run it with any Python 3
(see vision_controller/components/cnn/CMakeLists.txt). That's why the ops are read
from the .tflite flatbuffer directly instead of via tf.lite.experimental.Analyzer
(see extractMetadata.py), and the op -> resolver method mapping is taken from the
esp-tflite-micro headers the firmware is built against.
"""
import argparse
import re
import struct
import sys
from pathlib import Path

# Run as a script (model_helper/cExport.py), so the training root holding config.py isn't on sys.path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from config import MODEL_TFLITE_PATH, LABELS_PATH, ESP_MAIN_DIR

DEFAULT_TFLM_DIR = ESP_MAIN_DIR.parent.parent / "managed_components" / "espressif__esp-tflite-micro"
RESOLVER_HEADER = Path("tensorflow/lite/micro/micro_mutable_op_resolver.h")
SCHEMA_HEADER = Path("tensorflow/lite/schema/schema_generated.h")


class _FlatBuffer:
    """Just enough of a flatbuffer reader to walk tables, vectors and strings."""

    def __init__(self, data: bytes):
        self.data = data

    def u16(self, pos: int) -> int:
        return struct.unpack_from("<H", self.data, pos)[0]

    def u32(self, pos: int) -> int:
        return struct.unpack_from("<I", self.data, pos)[0]

    def i32(self, pos: int) -> int:
        return struct.unpack_from("<i", self.data, pos)[0]

    def root(self) -> int:
        return self.u32(0)

    def field(self, table: int, index: int) -> int | None:
        """Absolute position of field `index` of `table`, or None if it's absent (default value)."""
        vtable = table - self.i32(table)
        entry = 4 + 2 * index
        if entry >= self.u16(vtable):
            return None
        offset = self.u16(vtable + entry)
        return table + offset if offset else None

    def deref(self, pos: int) -> int:
        return pos + self.u32(pos)

    def tables(self, table: int, index: int) -> list[int]:
        pos = self.field(table, index)
        if pos is None:
            return []
        vector = self.deref(pos)
        return [self.deref(vector + 4 + 4 * i) for i in range(self.u32(vector))]

    def string(self, table: int, index: int) -> str | None:
        pos = self.field(table, index)
        if pos is None:
            return None
        string = self.deref(pos)
        return self.data[string + 4:string + 4 + self.u32(string)].decode()


def get_model_ops(tfLiteModelData: bytes) -> list[int]:
    """Builtin operator codes used by the model (tflite schema: Model.operator_codes)."""
    if tfLiteModelData[4:8] != b"TFL3":
        raise ValueError("not a .tflite model (missing TFL3 file identifier)")
    fb = _FlatBuffer(tfLiteModelData)
    codes = []
    # Model field 1: operator_codes; OperatorCode fields: 0 deprecated_builtin_code (int8),
    # 1 custom_code, 3 builtin_code (int32). Codes >= 127 only fit builtin_code, older
    # converters only write deprecated_builtin_code, so the actual code is the larger one.
    for op_code in fb.tables(fb.root(), 1):
        custom = fb.string(op_code, 1)
        if custom is not None:
            raise ValueError(f"custom op {custom!r} can't be registered automatically")
        deprecated = fb.field(op_code, 0)
        builtin = fb.field(op_code, 3)
        code = max(struct.unpack_from("<b", tfLiteModelData, deprecated)[0] if deprecated is not None else 0,
                   fb.i32(builtin) if builtin is not None else 0)
        if code not in codes:
            codes.append(code)
    return codes


def get_resolver_methods(tflm_dir: Path) -> dict[int, tuple[str, str]]:
    """Builtin operator code -> (BuiltinOperator name, MicroMutableOpResolver Add* method)."""
    schema = (tflm_dir / SCHEMA_HEADER).read_text()
    resolver = (tflm_dir / RESOLVER_HEADER).read_text()
    # aliases like BuiltinOperator_MIN = BuiltinOperator_ADD don't match \d+
    codes = {name: int(value) for name, value in re.findall(r"\bBuiltinOperator_(\w+) = (-?\d+)", schema)}
    methods = {}
    for method, name in re.findall(r"TfLiteStatus\s+(Add\w+)\s*\([^{}]*\)\s*\{\s*return\s+AddBuiltin\(\s*BuiltinOperator_(\w+)",
                                   resolver):
        methods.setdefault(codes[name], (name, method))
    return methods


def export_model_to_c(name: str, tfLiteModelData: bytes, labels: list[str], output_base: Path, tflm_dir: Path):
    """
    name: name of the model
    tfLiteModelData: tflite model data byte array
    labels: list of label names
    output_base: output base path
    tflm_dir: esp-tflite-micro component dir, for the op -> resolver method mapping
    """

    methods = get_resolver_methods(tflm_dir)
    ops = []
    for code in get_model_ops(tfLiteModelData):
        if code not in methods:
            raise ValueError(f"builtin op {code} isn't supported by MicroMutableOpResolver in {tflm_dir}")
        ops.append(methods[code])

    output_base.parent.mkdir(parents=True, exist_ok=True)
    header = output_base.with_suffix(".h")
    header.write_text(
        "#pragma once\n#include <cstdint>\n"
        '#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"\n\n'
        "extern const unsigned char g_model[];\n"
        "extern const unsigned int g_model_len;\n"
        "static const char* const kLabels[] = { \n"
        + ",\n".join(f'    "{label}"' for label in labels)
        + "\n};\n"
        f'static constexpr int kNumLabels = {len(labels)};\n\n'
        f"// ops used by the model: {', '.join(op for op, _ in ops)}\n"
        f"static constexpr int kNumModelOps = {len(ops)};\n"
        "using ModelOpResolver = tflite::MicroMutableOpResolver<kNumModelOps>;\n"
        "// registers every op of g_model with resolver\n"
        "TfLiteStatus registerModelOps(ModelOpResolver& resolver);\n"
    )
    lines = [", ".join(f"0x{b:02x}" for b in tfLiteModelData[i:i + 12]) for i in range(0, len(tfLiteModelData), 12)]
    output_base.with_suffix(".cc").write_text(
        f'#include "{header.name}"\n\n'
        f"// generated from {name} ({len(tfLiteModelData)} bytes)\n"
        "alignas(16) const unsigned char g_model[] = {\n  "
        + ",\n  ".join(lines)
        + "\n};\n"
          f"const unsigned int g_model_len = {len(tfLiteModelData)};\n\n"
          "TfLiteStatus registerModelOps(ModelOpResolver& resolver) {\n"
        + "".join(f"    if (TfLiteStatus status = resolver.{method}(); status != kTfLiteOk) return status;\n"
                  for _, method in ops)
        + "    return kTfLiteOk;\n}\n"
    )
    print(f"wrote {output_base}.h/.cc ({len(tfLiteModelData)} bytes, {len(ops)} ops)")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", default=str(MODEL_TFLITE_PATH), help=".tflite model to export")
    parser.add_argument("--labels", default=str(LABELS_PATH), help="text file with one label per line")
    parser.add_argument("--out", default=str(ESP_MAIN_DIR / "model_data"),
                        help="output base path, writes <out>.h and <out>.cc")
    parser.add_argument("--tflm-dir", default=str(DEFAULT_TFLM_DIR),
                        help="esp-tflite-micro component dir the firmware is built against")

    args = parser.parse_args()

    model = Path(args.model).read_bytes()
    labels = [line.strip() for line in Path(args.labels).read_text().splitlines() if line.strip()]
    export_model_to_c(name=Path(args.model).name, tfLiteModelData=model, labels=labels, output_base=Path(args.out),
                      tflm_dir=Path(args.tflm_dir))


if __name__ == "__main__":
    main()
