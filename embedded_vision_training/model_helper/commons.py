from pathlib import Path

from tensorflow import keras
from tensorflow.keras import layers

from constants import MODELS_DIR

AUGMENTATION = keras.Sequential(
    [
        layers.RandomFlip("horizontal"),
        layers.RandomRotation(0.08),
        layers.RandomZoom(0.15),
        layers.RandomContrast(0.15),
    ],
    name="augmentation",
)

def model_filenames(model_name: str, output_dir: Path | None = None) -> dict[str, Path]:
    if output_dir is None:
        output_dir = MODELS_DIR
    model_filename = output_dir / (model_name + ".keras")
    labels_filename = output_dir / (model_name + ".labels.txt")
    tflite_filename = output_dir / (model_name + ".tflite")
    data = {
        "model": model_filename,
        "labels": labels_filename,
        "tflite": tflite_filename
    }
    return data
