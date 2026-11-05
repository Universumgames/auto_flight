import argparse
import shutil
from pathlib import Path
import kagglehub

import cli_defaults

import constants
from dataset_helper import commons
from dataset_helper.commons import is_image, find_class_dirs


def download_ds_slug(slug: str, class_aliases: dict[str, list[str]] | None, raw_dir: Path | None) -> Path:
    """
    slug: kaggle hub slug to download
    class_aliases: aliases dict to merge multiple classes; folders not covered by it keep their own name
    raw_dir: "raw" directory to copy every image to per class, on this directory the dataset is eventually split into train, test and validate
    """
    print(f"Downloading '{slug}' via kagglehub ...")
    download_path = Path(kagglehub.dataset_download(slug))
    print(f"Downloaded to {download_path}")

    class_dirs = find_class_dirs(download_path, class_aliases)
    if not class_dirs:
        raise SystemExit(
            f"Could not find any folders containing images under {download_path}.\n"
            "Inspect that path yourself and copy images into data/raw/<class>/ by hand."
        )

    if raw_dir is None:
        raw_dir = commons.dataset_root_dir(slug)

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
                if is_image(f):
                    shutil.copy2(f, dst_dir / f"{prefix}_{f.name}")
                    n += 1
            print(f"  {src_dir} -> {dst_dir}/{prefix}_*")
        print(f"  {cls}: {n} files total")

    missing = set(class_aliases or {}) - set(class_dirs)
    if missing:
        print(f"\nNo folder found for: {sorted(missing)} -- add data/raw/{{{'/'.join(sorted(missing))}}} yourself.")

    print(
        "\nDone. Before training: open a handful of images in data/raw/ and confirm "
        "they're actually aerial/drone shots, not ground-level photos."
    )
    return raw_dir


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--slug", default=cli_defaults.DATASET_SLUG, help="Kaggle dataset slug, e.g. 'owner/dataset-name'.")
    parser.add_argument("--raw-dir", type=Path, default=cli_defaults.RAW_DIR)
    args = parser.parse_args()

    download_ds_slug(args.slug, cli_defaults.CLASS_ALIASES, args.raw_dir)
