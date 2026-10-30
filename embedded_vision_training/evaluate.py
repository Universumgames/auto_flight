"""Evaluate the quantized .tflite model on data/test/, to sanity-check that
int8 quantization didn't wreck accuracy before flashing it to the ESP32.
"""

import argparse

import numpy as np
import tensorflow as tf
from sklearn.metrics import classification_report, confusion_matrix

from config import IMAGE_SIZE, MODEL_TFLITE_PATH, TEST_DIR


def load_test_images(test_dir, image_size):
    ds = tf.keras.utils.image_dataset_from_directory(
        test_dir,
        image_size=(image_size, image_size),
        batch_size=1,
        shuffle=False,
    )
    class_names = ds.class_names
    images, labels = [], []
    for image_batch, label_batch in ds:
        images.append(image_batch.numpy()[0])
        labels.append(int(label_batch.numpy()[0]))
    return images, labels, class_names


def quantize_input(image_float, scale, zero_point):
    return np.clip(np.round(image_float / scale + zero_point), -128, 127).astype(np.int8)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", default=str(MODEL_TFLITE_PATH))
    args = parser.parse_args()

    if not TEST_DIR.is_dir():
        raise SystemExit("data/test not found -- run prepare_dataset.py first.")

    interpreter = tf.lite.Interpreter(model_path=args.model)
    interpreter.allocate_tensors()
    input_details = interpreter.get_input_details()[0]
    output_details = interpreter.get_output_details()[0]
    in_scale, in_zero_point = input_details["quantization"]

    images, labels, class_names = load_test_images(TEST_DIR, IMAGE_SIZE)
    print(f"Evaluating on {len(images)} test images, classes: {class_names}")

    predictions = []
    for image in images:
        quantized = quantize_input(image, in_scale, in_zero_point)
        interpreter.set_tensor(input_details["index"], quantized[np.newaxis, ...])
        interpreter.invoke()
        output = interpreter.get_tensor(output_details["index"])[0]
        predictions.append(int(np.argmax(output)))

    print("\n" + classification_report(labels, predictions, target_names=class_names))
    print("Confusion matrix (rows=true, cols=predicted):")
    print(class_names)
    print(confusion_matrix(labels, predictions))


if __name__ == "__main__":
    main()
