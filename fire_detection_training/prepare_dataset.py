"""Split data/raw/<class>/*.jpg into data/{train,val,test}/<class>/.

Expected input layout (create this yourself, or run download_dataset.py
for a starting point):

    data/raw/fire/*.jpg
    data/raw/smoke/*.jpg
    data/raw/none/*.jpg

Class folder names become the model's labels, in sorted order. You need at
least two classes; "none"/negative examples matter as much as positives.
"""

import argparse
import random
import shutil
from pathlib import Path

from config import RAW_DIR, SPLIT_SEED, TEST_DIR, TRAIN_DIR, TRAIN_VAL_TEST_SPLIT, VAL_DIR

IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp"}


def split_class(class_dir: Path, train_ratio: float, val_ratio: float, seed: int):
    images = sorted(p for p in class_dir.iterdir() if p.suffix.lower() in IMAGE_EXTENSIONS)
    if not images:
        raise ValueError(f"No images found in {class_dir}")

    rng = random.Random(seed)
    rng.shuffle(images)

    n_train = int(len(images) * train_ratio)
    n_val = int(len(images) * val_ratio)

    return {
        "train": images[:n_train],
        "val": images[n_train : n_train + n_val],
        "test": images[n_train + n_val :],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw-dir", type=Path, default=RAW_DIR)
    parser.add_argument("--copy", action="store_true", help="Copy files instead of symlinking (default: symlink).")
    args = parser.parse_args()

    if not args.raw_dir.is_dir():
        raise SystemExit(f"{args.raw_dir} does not exist. Create data/raw/<class>/*.jpg first.")

    class_dirs = sorted(p for p in args.raw_dir.iterdir() if p.is_dir())
    if len(class_dirs) < 2:
        raise SystemExit(f"Found {len(class_dirs)} class folders under {args.raw_dir}, need at least 2.")

    train_ratio, val_ratio, _test_ratio = TRAIN_VAL_TEST_SPLIT
    split_dirs = {"train": TRAIN_DIR, "val": VAL_DIR, "test": TEST_DIR}

    for split_name, split_dir in split_dirs.items():
        if split_dir.exists():
            shutil.rmtree(split_dir)

    for class_dir in class_dirs:
        splits = split_class(class_dir, train_ratio, val_ratio, SPLIT_SEED)
        for split_name, files in splits.items():
            out_dir = split_dirs[split_name] / class_dir.name
            out_dir.mkdir(parents=True, exist_ok=True)
            for src in files:
                dst = out_dir / src.name
                if args.copy:
                    shutil.copy2(src, dst)
                else:
                    dst.symlink_to(src.resolve())
            print(f"{class_dir.name:>8} / {split_name:<5}: {len(files)} images")

    print(f"\nDone. Classes: {[d.name for d in class_dirs]}")


if __name__ == "__main__":
    main()
