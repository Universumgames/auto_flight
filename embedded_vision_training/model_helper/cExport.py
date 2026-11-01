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
import sys
from pathlib import Path

# Run as a script (model_helper/cExport.py), so the training root holding config.py and the
# model_helper package isn't on sys.path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from config import MODEL_TFLITE_PATH, LABELS_PATH, ESP_MAIN_DIR
from model_helper.extractMetadata import get_model_ops

DEFAULT_TFLM_DIR = ESP_MAIN_DIR.parent.parent / "managed_components" / "espressif__esp-tflite-micro"
RESOLVER_HEADER = Path("tensorflow/lite/micro/micro_mutable_op_resolver.h")
SCHEMA_HEADER = Path("tensorflow/lite/schema/schema_generated.h")


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
        f"static const char* modelName = \"{name}\"\n"
        "static const char* kLabels[] = { \n"
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
