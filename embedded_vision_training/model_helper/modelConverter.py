"""Convert the trained Keras model to a fully int8-quantized .tflite model.

Full integer quantization (int8 in, int8 out, int8 weights/activations) is
required for TensorFlow Lite Micro on ESP32: there's no FPU-friendly path
for float32 inference at any useful framerate, and the int8 model is ~4x
smaller than float32.
"""
import argparse
from pathlib import Path

import tensorflow as tf

def representative_dataset(train_dir: Path, image_size: int, num_samples: int):
    """
    Get a representative dataset for training or quantisation
    train_dir: path to the training data directory
    image_size: the image size
    num_samples: the number of samples
    """
    train_ds = tf.keras.utils.image_dataset_from_directory(
        train_dir,
        image_size=(image_size, image_size),
        batch_size=1,
        shuffle=True,
    )

    for images, _ in train_ds.unbatch().batch(1).take(num_samples):
        yield [tf.cast(images, tf.float32)]

# PyCharm can't statically resolve tf.lite.* / tf.int8 (TensorFlow lazy-loads them)
# noinspection PyUnresolvedReferences
def quantize_model(model: keras.Model, train_dir: Path, image_size: int) -> bytes:
    """
    Quantize the model for embedded use
    model: source Model
    train_dir: path to the training data directory
    image_size: the image size
    @return: serialized quantized .tflite flatbuffer
    """
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8  # input tensor is int8
    converter.inference_output_type = tf.int8  # output tensor is int8

    tflite_model = converter.convert()

    return tflite_model

# noinspection PyUnresolvedReferences
def export_quantized(model: keras.Model, train_dir: Path, image_size: int, dest: Path) -> None:
    """
    Create a quantized model for embedded use
    model: source Model
    train_dir: path to the training data directory
    image_size: the image size
    dest: path to the quantized model
    """
    tflite_model = quantize_model(model, train_dir, image_size)

    with open(dest, "wb") as f:
        f.write(tflite_model)

    interpreter = tf.lite.Interpreter(model_content=tflite_model)
    input_details = interpreter.get_input_details()[0]
    output_details = interpreter.get_output_details()[0]

    print(f"Wrote {dest} ({len(tflite_model) / 1024:.1f} KB)")
    print(f"Input:  dtype={input_details['dtype'].__name__}, shape={input_details['shape']}, "
          f"scale={input_details['quantization'][0]:.6f}, zero_point={input_details['quantization'][1]}")
    print(f"Output: dtype={output_details['dtype'].__name__}, shape={output_details['shape']}, "
          f"scale={output_details['quantization'][0]:.6f}, zero_point={output_details['quantization'][1]}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", type=Path)
    parser.add_argument("--output", type=Path, nargs="?")
    parser.add_argument("--train-dir", type=Path)
    parser.add_argument("--image-size", type=int)
    args = parser.parse_args()

    model = tf.keras.models.load_model(args.model)
    output_file = args.output if args.output is not None else args.model.parent / (args.model.stem + ".tflite")

    export_quantized(model, args.train_dir, args.image_size, output_file)


if __name__ == "__main__":
    main()
