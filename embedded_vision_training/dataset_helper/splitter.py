import argparse
import os
import random
import shutil
from pathlib import Path

import cli_defaults

IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp"}


def _split_class(class_dir: Path, train_ratio: float, val_ratio: float, seed: int):
    """
    Split Dataset class into training, validation and testing
    class_dir: directory of the class to splot
    train_ratio: percentage of how many images should be used for training
    val_ration: percentage of how many images should be used for validation
    seed: seed for the RNG, used for shuffling the images before splitting
    """
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

def split_exists(output_dir: Path) -> bool:
    """
    Check if a splitted dataset is already present at that directory
    """
    return (output_dir / "train").is_dir() and (output_dir / "val").is_dir() and (output_dir / "test").is_dir()

def split_dataset(raw_dir: Path, output_dir: Path, train_ratio: float, val_ratio: float, seed: int, copy: bool = True):
    """
    Split dataset into training, validation and testing data
    If the output directories already exist (aka. previous splits) they are removed and a new split is created
    raw_dir: source directory containing the dataset split into classes
    output_dir: output directory to put the splitted dataset
    train_ratio: percentage of how many images should be used for training
    val_ration: percentage of how many images should be used for validation
    seed: seed for the RNG, used for shuffling the images before splitting
    copy: if True files are copied into splitted directory, if False symlinks are created
    """
    if not raw_dir.is_dir():
        raise SystemExit(f"{raw_dir} does not exist. Create data/raw/<class>/*.jpg first.")

    class_dirs = sorted(p for p in raw_dir.iterdir() if p.is_dir())
    if len(class_dirs) < 2:
        raise SystemExit(f"Found {len(class_dirs)} class folders under {raw_dir}, need at least 2.")

    _test_ratio = 1 - train_ratio - val_ratio
    split_dirs = {"train": output_dir / "train", "val": output_dir / "val", "test": output_dir / "test"}

    if not output_dir.is_dir():
        os.mkdir(output_dir)

    for split_name, split_dir in split_dirs.items():
        if split_dir.exists():
            shutil.rmtree(split_dir)

    for class_dir in class_dirs:
        splits = _split_class(class_dir, train_ratio, val_ratio, seed)
        for split_name, files in splits.items():
            out_dir = split_dirs[split_name] / class_dir.name
            out_dir.mkdir(parents=True, exist_ok=True)
            for src in files:
                dst = out_dir / src.name
                if copy:
                    shutil.copy2(src, dst)
                else:
                    dst.symlink_to(src.resolve())
            print(f"{class_dir.name:>8} / {split_name:<5}: {len(files)} images")

    print(f"\nDone. Classes: {[d.name for d in class_dirs]}")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw-dir", type=Path, default=cli_defaults.RAW_DIR)
    parser.add_argument("--out-dir", type=Path, default=cli_defaults.DATA_DIR)
    parser.add_argument("--copy", action="store_true", help="Copy files instead of symlinking (default: symlink).")
    parser.add_argument("--split-relation", nargs="+", typle=float, default=list(cli_defaults.TRAIN_VAL_TEST_SPLIT), help="Split relation of train, validation and test")
    parser.add_argument("--seed", default=cli_defaults.SPLIT_SEED)
    args = parser.parse_args()

    train_ratio, val_ratio, _test_ratio = tuple(args.split_relation)
    split_dataset(args.raw_dir, args.out_dir, train_ratio, val_ratio, args.seed, True)

if __name__ == "__main__":
    main()
