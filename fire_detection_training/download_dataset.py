"""Fetch a starter aerial/drone fire+smoke dataset from Kaggle into data/raw/.

This project is for a fixed-wing drone looking DOWN at fields, so the
dataset needs to be bird's-eye imagery, not ground-level photos of fires
(most "fire dataset" hits on Kaggle are the latter -- e.g. phylake1337's
popular "Fire Dataset" is close-up ground photography and is the WRONG
domain for this use case, even though it's an easy first hit).

The default below points at a Kaggle repackaging of the FLAME dataset
(Fire Luminosity Airborne-based Machine learning Evaluation, Northern
Arizona University): a real UAV/drone dataset of prescribed forest-fire
footage. The original FLAME is distributed via IEEE DataPort and needs a
free account + manual download; this Kaggle mirror is easier to script but
its exact internal folder names could differ or drift, which is why the
matching logic below is alias-based/fuzzy rather than a fixed path. After
downloading, LOOK AT A FEW IMAGES in data/raw/ to confirm they're actually
aerial before you spend time training on them.

If this dataset stops working or the images don't look aerial:
  - search Kaggle for "FLAME wildfire dataset", "FlameVision", or "aerial
    wildfire smoke dataset", or
  - get the original FLAME / FLAME 2 dataset from IEEE DataPort (FLAME 2 in
    particular has real Fire AND Smoke labels, not just Fire/No-Fire):
    https://ieee-dataport.org/open-access/flame-dataset-aerial-imagery-pile-burn-detection-using-drones-uavs
    https://ieee-dataport.org/open-access/flame-2-fire-detection-and-modeling-aerial-multi-spectral-image-dataset
  - or skip this script entirely and populate data/raw/<class>/*.jpg by hand
    from any aerial/drone source you trust.

Requires a Kaggle account and API token (~/.kaggle/kaggle.json), see
https://www.kaggle.com/docs/api for how to create one.
"""

import argparse
import re
import shutil
from pathlib import Path

from config import RAW_DIR

DATASET_SLUG = "warcoder/flamevision-dataset-for-wildfire-classification"

# Folder-name aliases we'll look for anywhere in the downloaded tree,
# matched case-insensitively after stripping non-alphanumeric characters
# (so "No_Fire", "no-fire", "NoFire" all match the same alias).
CLASS_ALIASES = {
    "fire": ["fire"],
    "smoke": ["smoke"],
    "none": ["nofire", "nonfire", "no", "normal", "none", "negative", "background"],
}


def normalize(name: str) -> str:
    return re.sub(r"[^a-z0-9]", "", name.lower())


def find_class_dirs(download_path: Path):
    """Find every source directory per class among all subdirectories.

    Some datasets (e.g. the default FLAME-derived one) ship their own
    train/valid/test split, each with its own fire/nofire/... subfolders.
    We deliberately flatten all of those together into data/raw/<class>/:
    prepare_dataset.py does its own train/val/test split downstream, and
    keeping the source split separate would just waste held-out images.
    """
    normalized_aliases = {
        cls: {normalize(alias) for alias in aliases} for cls, aliases in CLASS_ALIASES.items()
    }

    found = {}
    for d in download_path.rglob("*"):
        if not d.is_dir():
            continue
        norm = normalize(d.name)
        for cls, aliases in normalized_aliases.items():
            if norm in aliases:
                # Prefer directories that actually contain image files.
                if any(f.suffix.lower() in {".jpg", ".jpeg", ".png"} for f in d.iterdir() if f.is_file()):
                    found.setdefault(cls, []).append(d)
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--slug", default=DATASET_SLUG, help="Kaggle dataset slug, e.g. 'owner/dataset-name'.")
    parser.add_argument("--raw-dir", type=Path, default=RAW_DIR)
    args = parser.parse_args()

    import kagglehub

    print(f"Downloading '{args.slug}' via kagglehub ...")
    download_path = Path(kagglehub.dataset_download(args.slug))
    print(f"Downloaded to {download_path}")

    class_dirs = find_class_dirs(download_path)
    if not class_dirs:
        raise SystemExit(
            f"Could not find any fire/smoke/none-like folders under {download_path}.\n"
            "Inspect that path yourself and either rename folders to match CLASS_ALIASES "
            "in this script, or copy images into data/raw/<class>/ by hand."
        )

    args.raw_dir.mkdir(parents=True, exist_ok=True)
    for cls, src_dirs in class_dirs.items():
        dst_dir = args.raw_dir / cls
        dst_dir.mkdir(parents=True, exist_ok=True)
        n = 0
        for src_dir in src_dirs:
            # Prefix with the parent folder name (e.g. "train", "valid") so
            # files with the same name in different source splits don't
            # overwrite each other.
            prefix = src_dir.parent.name
            for f in src_dir.iterdir():
                if f.is_file() and f.suffix.lower() in {".jpg", ".jpeg", ".png"}:
                    shutil.copy2(f, dst_dir / f"{prefix}_{f.name}")
                    n += 1
            print(f"  {src_dir} -> {dst_dir}/{prefix}_*")
        print(f"  {cls}: {n} files total")

    missing = set(CLASS_ALIASES) - set(class_dirs)
    if missing:
        print(f"\nNo folder found for: {sorted(missing)} -- add data/raw/{{{'/'.join(sorted(missing))}}} yourself.")

    print(
        "\nDone. Before training: open a handful of images in data/raw/ and confirm "
        "they're actually aerial/drone shots, not ground-level photos."
    )


if __name__ == "__main__":
    main()
