"""Evaluate the quantized .tflite model on data/test/, to sanity-check that
int8 quantization didn't wreck accuracy before flashing it to the ESP32.
"""

import argparse

import numpy as np
from ai_edge_litert.interpreter import Interpreter
from sklearn.metrics import classification_report, confusion_matrix

from dataset_helper import load
from pathlib import Path


def quantize_input(image_float, scale, zero_point):
    return np.clip(np.round(image_float / scale + zero_point), -128, 127).astype(np.int8)

def evaluate_tflite_model(tfLiteModel: Path, test_dir: Path, image_size: int):
    """
    Evaluate a tf lite model, results are printed to the console
    tfLiteModel: file path to the tf lite model
    test_dir: Path to test dataset
    image_size: the image_size
    """
    interpreter = Interpreter(model_path=str(tfLiteModel))
    interpreter.allocate_tensors()
    input_details = interpreter.get_input_details()[0]
    output_details = interpreter.get_output_details()[0]
    in_scale, in_zero_point = input_details["quantization"]

    images, labels, class_names = load.load_test_images(test_dir, image_size)
    print(f"Evaluating on {len(images)} test images, classes: {class_names}")

    predictions = []
    for image in images:
        quantized = quantize_input(image, in_scale, in_zero_point)
        interpreter.set_tensor(input_details["index"], quantized[np.newaxis, ...])
        interpreter.invoke()
        output = interpreter.get_tensor(output_details["index"])[0]
        predictions.append(int(np.argmax(output)))

    report = classification_report(labels, predictions, target_names=class_names)
    cm = confusion_matrix(labels, predictions)
    print()
    print(report)
    print("Confusion matrix (rows=true, cols=predicted):")
    print(class_names)
    print(cm)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", type=Path, help="Path to tflite model")
    parser.add_argument("--test-dir", type=Path, help="Path to test data directory")
    parser.add_argument("--image-size", type=int, help="Image size")
    args = parser.parse_args()

    test_dir = args.test_dir

    if not test_dir.is_dir():
        raise SystemExit("data/test not found -- run prepare_dataset.py first.")

    evaluate_tflite_model(args.model, test_dir, args.image_size)


if __name__ == "__main__":
    main()
