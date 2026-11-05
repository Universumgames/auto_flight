import re
from pathlib import Path

from constants import DATASETS_DIR


def _normalize(name: str) -> str:
    return re.sub(r"[^a-z0-9]", "", name.lower())

def slug_to_id(slug: str) -> str:
    return slug.replace("/", "_")

IMAGE_SUFFIXES = {".jpg", ".jpeg", ".png"}

def is_image(f: Path) -> bool:
    return f.is_file() and f.suffix.lower() in IMAGE_SUFFIXES

def dataset_root_dir(slug: str, datasets_dir: Path | None = None):
    if datasets_dir is None:
        datasets_dir = DATASETS_DIR
    slug_id = slug_to_id(slug)
    ds_dir = datasets_dir / slug_id
    return ds_dir

def splitted_dataset_dirs(slug: str, root: Path | None = None):
    ds_dir = dataset_root_dir(slug, root)
    train_path = ds_dir / "train"
    val_path = ds_dir / "val"
    test_path = ds_dir / "test"
    data = {
        "train": train_path,
        "test": test_path,
        "val": val_path
    }

def find_class_dirs(download_path: Path, class_aliases: dict[str, list[str]] | None = None) -> dict[str, list[Path]]:
    """Find every source directory per class among all subdirectories.

    Some datasets (e.g. the default FLAME-derived one) ship their own
    train/valid/test split, each with its own fire/nofire/... subfolders.
    We deliberately flatten all of those together into data/raw/<class>/:
    prepare_dataset.py does its own train/val/test split downstream, and
    keeping the source split separate would just waste held-out images.

    Folder names are matched against classes after normalizing them:
    1. an explicit alias, e.g. {"none": ["nofire"]} maps "nofire" to "none"
    2. a class name, e.g. {"fire": []} maps "fire" to "fire"
    3. otherwise the folder keeps its own name as class, so None / {} keeps
       the dataset's classes as-is.
    """
    class_aliases = class_aliases or {}

    # normalized folder name -> class name
    class_of = {}
    for cls, aliases in class_aliases.items():
        for alias in aliases:
            class_of.setdefault(_normalize(alias), cls)
    for cls in class_aliases:
        # setdefault: an explicit alias of another class takes precedence
        class_of.setdefault(_normalize(cls), cls)

    found = {}
    for d in sorted(download_path.rglob("*")):
        if not d.is_dir() or not any(is_image(f) for f in d.iterdir()):
            continue
        # Unknown folders become their own class; storing the first seen name
        # merges e.g. train/Smoke and valid/smoke into one class.
        cls = class_of.setdefault(_normalize(d.name), d.name)
        found.setdefault(cls, []).append(d)
    return found
