from constants import ROOT_DIR

DATASET_SLUG = "warcoder/flamevision-dataset-for-wildfire-classification"

CLASS_ALIASES = {
    "fire": [],
    "smoke": [],
    "none": ["nofire", "nonfire", "no", "normal", "negative", "background"],
}

# dataset dirs
DATA_DIR = ROOT_DIR / "data"
RAW_DIR = DATA_DIR/ "raw"
TRAIN_DIR = ROOT_DIR / "data" / "train"
VAL_DIR = ROOT_DIR / "data" / "val"
TEST_DIR = ROOT_DIR / "data" / "test"

TRAIN_VAL_TEST_SPLIT = (0.7, 0.15, 0.15)
SPLIT_SEED = 42

IMAGE_SIZE = 96  # width == height
CHANNELS = 3