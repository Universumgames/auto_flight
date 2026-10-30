"""Convert the trained Keras model to a fully int8-quantized .tflite model.

Full integer quantization (int8 in, int8 out, int8 weights/activations) is
required for TensorFlow Lite Micro on ESP32: there's no FPU-friendly path
for float32 inference at any useful framerate, and the int8 model is ~4x
smaller than float32.
"""

import argparse
import tempfile

import tensorflow as tf
from tensorflow import keras

from config import IMAGE_SIZE, MODEL_KERAS_PATH, MODEL_TFLITE_PATH, TRAIN_DIR

NUM_REPRESENTATIVE_SAMPLES = 200


def representative_dataset(train_dir, image_size, num_samples):
    ds = keras.utils.image_dataset_from_directory(
        train_dir,
        image_size=(image_size, image_size),
        batch_size=1,
        shuffle=True,
    )

    def gen():
        count = 0
        for images, _labels in ds:
            if count >= num_samples:
                return
            yield [tf.cast(images, tf.float32)]
            count += 1

    return gen


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", default=str(MODEL_KERAS_PATH))
    parser.add_argument("--output", default=str(MODEL_TFLITE_PATH))
    args = parser.parse_args()

    model = keras.models.load_model(args.model)

    # TFLiteConverter.from_keras_model() chokes on Keras 3 functional models
    # in this TF/Keras combo (LLVM "missing attribute 'value'" on a
    # BatchNormalization ReadVariableOp/Cast). Exporting to a plain
    # SavedModel first and converting from that sidesteps it.
    with tempfile.TemporaryDirectory() as tmp_dir:
        model.export(tmp_dir)
        converter = tf.lite.TFLiteConverter.from_saved_model(tmp_dir)
        converter.optimizations = [tf.lite.Optimize.DEFAULT]
        converter.representative_dataset = representative_dataset(
            TRAIN_DIR, IMAGE_SIZE, NUM_REPRESENTATIVE_SAMPLES
        )
        converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
        converter.inference_input_type = tf.int8
        converter.inference_output_type = tf.int8

        tflite_model = converter.convert()

    out_path = args.output
    with open(out_path, "wb") as f:
        f.write(tflite_model)

    interpreter = tf.lite.Interpreter(model_content=tflite_model)
    input_details = interpreter.get_input_details()[0]
    output_details = interpreter.get_output_details()[0]

    print(f"Wrote {out_path} ({len(tflite_model) / 1024:.1f} KB)")
    print(f"Input:  dtype={input_details['dtype'].__name__}, shape={input_details['shape']}, "
          f"scale={input_details['quantization'][0]:.6f}, zero_point={input_details['quantization'][1]}")
    print(f"Output: dtype={output_details['dtype'].__name__}, shape={output_details['shape']}, "
          f"scale={output_details['quantization'][0]:.6f}, zero_point={output_details['quantization'][1]}")


if __name__ == "__main__":
    main()
