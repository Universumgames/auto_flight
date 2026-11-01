import argparse
import re
import shutil
from pathlib import Path
import kagglehub

import cli_defaults

def normalize(name: str) -> str:
    return re.sub(r"[^a-z0-9]", "", name.lower())


def find_class_dirs(download_path: Path, class_aliases: dict[str, list[str]]):
    """Find every source directory per class among all subdirectories.

    Some datasets (e.g. the default FLAME-derived one) ship their own
    train/valid/test split, each with its own fire/nofire/... subfolders.
    We deliberately flatten all of those together into data/raw/<class>/:
    prepare_dataset.py does its own train/val/test split downstream, and
    keeping the source split separate would just waste held-out images.
    """
    normalized_aliases = {
        cls: {normalize(alias) for alias in aliases} for cls, aliases in class_aliases.items()
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


def download_ds_slug(slug: str, class_aliases: dict[str, list[str]], raw_dir: Path):
    """
    slug: kaggle hub slug to download
    class_aliases: aliases dict to merge multiple classes
    raw_dir: "raw" directory to copy every image to per class, on this directory the dataset is eventually split into train, test and validate
    """
    print(f"Downloading '{slug}' via kagglehub ...")
    download_path = Path(kagglehub.dataset_download(slug))
    print(f"Downloaded to {download_path}")

    class_dirs = find_class_dirs(download_path)
    if not class_dirs:
        raise SystemExit(
            f"Could not find any fire/smoke/none-like folders under {download_path}.\n"
            "Inspect that path yourself and either rename folders to match CLASS_ALIASES "
            "in this script, or copy images into data/raw/<class>/ by hand."
        )

    raw_dir.mkdir(parents=True, exist_ok=True)
    for cls, src_dirs in class_dirs.items():
        dst_dir = raw_dir / cls
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

    missing = set(class_aliases) - set(class_dirs)
    if missing:
        print(f"\nNo folder found for: {sorted(missing)} -- add data/raw/{{{'/'.join(sorted(missing))}}} yourself.")

    print(
        "\nDone. Before training: open a handful of images in data/raw/ and confirm "
        "they're actually aerial/drone shots, not ground-level photos."
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--slug", default=cli_defaults.DATASET_SLUG, help="Kaggle dataset slug, e.g. 'owner/dataset-name'.")
    parser.add_argument("--raw-dir", type=Path, default=cli_defaults.RAW_DIR)
    args = parser.parse_args()

    download_ds_slug(args.slug, cli_defaults.CLASS_ALIASES, args.raw_dir)
