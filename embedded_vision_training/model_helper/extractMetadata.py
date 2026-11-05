import argparse
import struct
from pathlib import Path

from model_helper.FlatBuffer import FlatBuffer

def log_model_metadata(tfLiteModelFile: Path):
    # Imported here, so get_model_ops stays stdlib only for cExport.py (run by the vision_controller build)
    import tensorflow
    tensorflow.lite.experimental.Analyzer.analyze(model_path=tfLiteModelFile)

def get_model_ops(tfLiteModelData: bytes) -> list[int]:
    """Builtin operator codes used by the model (tflite schema: Model.operator_codes)."""
    if tfLiteModelData[4:8] != b"TFL3":
        raise ValueError("not a .tflite model (missing TFL3 file identifier)")
    fb = FlatBuffer(tfLiteModelData)
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

def get_model_ops_from_file(tfLiteModelPath: Path) -> list[int]:
    return get_model_ops(tfLiteModelPath.read_bytes())

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-m", "--model", type=Path, help=".tflite model to get metadata out of")

    args = parser.parse_args()

    log_model_metadata(args.model)

if __name__ == "__main__":
    main()