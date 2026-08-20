"""Shared constants for the fire/smoke detection training pipeline."""

from pathlib import Path

ROOT_DIR = Path(__file__).parent
RAW_DIR = ROOT_DIR / "data" / "raw"
TRAIN_DIR = ROOT_DIR / "data" / "train"
VAL_DIR = ROOT_DIR / "data" / "val"
TEST_DIR = ROOT_DIR / "data" / "test"
MODELS_DIR = ROOT_DIR / "models"

# Keep this small: the model runs on an ESP32 with a few hundred KB of RAM
# for the whole interpreter + tensor arena, not just the weights.
IMAGE_SIZE = 96  # width == height
CHANNELS = 3

TRAIN_VAL_TEST_SPLIT = (0.7, 0.15, 0.15)
SPLIT_SEED = 42

MODEL_KERAS_PATH = MODELS_DIR / "fire_smoke_cnn.keras"
LABELS_PATH = MODELS_DIR / "labels.txt"
MODEL_TFLITE_PATH = MODELS_DIR / "fire_smoke_cnn_int8.tflite"

# Where export_c_array.py writes the generated C source by default.
ESP_MAIN_DIR = ROOT_DIR.parent / "fire_detection_esp" / "main"
