"""Evaluate .keras and/or int8 .tflite models on a held-out test directory.

Produces comparable numbers per model:
  * accuracy, balanced accuracy, macro F1 (with bootstrap 95% CIs)
  * per-class precision / recall / F1
  * for the positive class (default "fire"): recall (= detection rate),
    miss rate, false alarm rate, ROC-AUC and PR-AUC from the class score
  * confusion matrix
  * model size (file bytes) and, for .keras models, parameter count
  * when a .keras and a .tflite model are evaluated together: prediction
    agreement and the accuracy drop caused by int8 quantization

Every run writes a full JSON report to <out-dir>/<model>.<kind>.json and
appends one flat row to <out-dir>/results.csv, so runs can be compared later.

Example:
    uv run -m model_helper.evaluate --model models/debug.keras models/debug.tflite
"""

import argparse
import csv
import json
from datetime import datetime
from pathlib import Path

import numpy as np
from ai_edge_litert.interpreter import Interpreter
from sklearn.metrics import (
    accuracy_score,
    average_precision_score,
    balanced_accuracy_score,
    confusion_matrix,
    f1_score,
    precision_recall_fscore_support,
    roc_auc_score,
)

import cli_defaults
from constants import EVALUATIONS_DIR
from dataset_helper import load

BOOTSTRAP_SAMPLES = 1000
BOOTSTRAP_SEED = 0

CSV_COLUMNS = [
    "timestamp", "model", "kind", "tag", "test_dir", "n_images",
    "accuracy", "accuracy_ci_low", "accuracy_ci_high",
    "balanced_accuracy", "macro_f1", "macro_f1_ci_low", "macro_f1_ci_high",
    "positive_class", "pos_precision", "pos_recall", "pos_recall_ci_low", "pos_recall_ci_high",
    "false_alarm_rate", "roc_auc", "pr_auc",
    "size_kb", "params", "agreement_with_keras", "quantization_accuracy_drop",
]


# ---------------------------------------------------------------- inference

def quantize_input(image_float, scale, zero_point):
    return np.clip(np.round(image_float / scale + zero_point), -128, 127).astype(np.int8)


def predict_tflite(model_path: Path, images: np.ndarray) -> np.ndarray:
    """Run the int8 model image by image (like on the ESP32); returns dequantized class scores."""
    interpreter = Interpreter(model_path=str(model_path))
    interpreter.allocate_tensors()
    input_details = interpreter.get_input_details()[0]
    output_details = interpreter.get_output_details()[0]
    in_scale, in_zero_point = input_details["quantization"]
    out_scale, out_zero_point = output_details["quantization"]

    scores = []
    for image in images:
        interpreter.set_tensor(input_details["index"], quantize_input(image, in_scale, in_zero_point)[np.newaxis, ...])
        interpreter.invoke()
        raw = interpreter.get_tensor(output_details["index"])[0].astype(np.float32)
        scores.append((raw - out_zero_point) * out_scale if out_scale else raw)
    return np.stack(scores)


def predict_keras(model_path: Path, images: np.ndarray) -> tuple[np.ndarray, int]:
    """Returns class scores and the parameter count."""
    from tensorflow import keras  # lazy: TF import is slow and not needed for tflite-only runs

    model = keras.models.load_model(model_path)
    return model.predict(images, batch_size=64, verbose=0), model.count_params()


# ---------------------------------------------------------------- metrics

def _bootstrap_ci(labels: np.ndarray, predictions: np.ndarray, metric, rng: np.random.Generator) -> tuple[float, float]:
    n = len(labels)
    values = []
    for _ in range(BOOTSTRAP_SAMPLES):
        idx = rng.integers(0, n, n)
        values.append(metric(labels[idx], predictions[idx]))
    low, high = np.nanpercentile(values, [2.5, 97.5])
    return float(low), float(high)


def compute_metrics(labels: np.ndarray, scores: np.ndarray, class_names: list[str], positive_class: str | None) -> dict:
    predictions = scores.argmax(axis=1)
    class_ids = list(range(len(class_names)))
    rng = np.random.default_rng(BOOTSTRAP_SEED)

    macro_f1 = lambda y, p: f1_score(y, p, labels=class_ids, average="macro", zero_division=0)
    precision, recall, f1, support = precision_recall_fscore_support(
        labels, predictions, labels=class_ids, zero_division=0
    )

    metrics = {
        "n_images": int(len(labels)),
        "accuracy": float(accuracy_score(labels, predictions)),
        "accuracy_ci95": _bootstrap_ci(labels, predictions, accuracy_score, rng),
        "balanced_accuracy": float(balanced_accuracy_score(labels, predictions)),
        "macro_f1": float(macro_f1(labels, predictions)),
        "macro_f1_ci95": _bootstrap_ci(labels, predictions, macro_f1, rng),
        "per_class": {
            name: {"precision": float(precision[i]), "recall": float(recall[i]), "f1": float(f1[i]),
                   "support": int(support[i])}
            for i, name in enumerate(class_names)
        },
        "confusion_matrix": confusion_matrix(labels, predictions, labels=class_ids).tolist(),
    }

    if positive_class in class_names:
        pos = class_names.index(positive_class)
        is_pos = labels == pos
        pred_pos = predictions == pos
        pos_recall = lambda y, p: np.mean(p[y == pos] == pos) if np.any(y == pos) else np.nan
        metrics["positive_class"] = {
            "name": positive_class,
            "precision": float(precision[pos]),
            "recall": float(recall[pos]),
            "recall_ci95": _bootstrap_ci(labels, predictions, pos_recall, rng),
            "miss_rate": float(1 - recall[pos]),
            # share of negative images that raise a (false) alarm
            "false_alarm_rate": float(np.mean(pred_pos[~is_pos])) if np.any(~is_pos) else float("nan"),
            # threshold-independent: how well the positive-class score ranks positives above negatives
            "roc_auc": float(roc_auc_score(is_pos, scores[:, pos])) if 0 < is_pos.sum() < len(labels) else float("nan"),
            "pr_auc": float(average_precision_score(is_pos, scores[:, pos])) if is_pos.any() else float("nan"),
        }
    return metrics


# ---------------------------------------------------------------- reporting

def print_report(name: str, m: dict, class_names: list[str]):
    lo, hi = m["accuracy_ci95"]
    print(f"\n=== {name} ({m['n_images']} images) ===")
    print(f"accuracy          {m['accuracy']:.4f}  [95% CI {lo:.4f}-{hi:.4f}]")
    print(f"balanced accuracy {m['balanced_accuracy']:.4f}")
    lo, hi = m["macro_f1_ci95"]
    print(f"macro F1          {m['macro_f1']:.4f}  [95% CI {lo:.4f}-{hi:.4f}]")
    if "positive_class" in m:
        p = m["positive_class"]
        lo, hi = p["recall_ci95"]
        print(f"'{p['name']}' recall      {p['recall']:.4f}  [95% CI {lo:.4f}-{hi:.4f}]  (miss rate {p['miss_rate']:.4f})")
        print(f"'{p['name']}' precision   {p['precision']:.4f}")
        print(f"false alarm rate  {p['false_alarm_rate']:.4f}")
        print(f"ROC-AUC / PR-AUC  {p['roc_auc']:.4f} / {p['pr_auc']:.4f}")
    print(f"\n{'class':>10} {'prec':>7} {'recall':>7} {'f1':>7} {'n':>6}")
    for cls, v in m["per_class"].items():
        print(f"{cls:>10} {v['precision']:7.4f} {v['recall']:7.4f} {v['f1']:7.4f} {v['support']:6d}")
    print("\nconfusion matrix (rows=true, cols=predicted):", class_names)
    for cls, row in zip(class_names, m["confusion_matrix"]):
        print(f"{cls:>10} {row}")
    if "size_kb" in m:
        params = f", {m['params']:,} params" if m.get("params") else ""
        print(f"\nmodel size {m['size_kb']:.1f} KB{params}")
    if "agreement_with_keras" in m:
        print(f"agreement with keras {m['agreement_with_keras']:.4f}, "
              f"quantization accuracy drop {m['quantization_accuracy_drop']:+.4f}")


def save_results(out_dir: Path, report: dict):
    out_dir.mkdir(parents=True, exist_ok=True)
    json_path = out_dir / f"{report['model']}.{report['kind']}.json"
    json_path.write_text(json.dumps(report, indent=2))

    m = report["metrics"]
    p = m.get("positive_class", {})
    row = {
        "timestamp": report["timestamp"], "model": report["model"], "kind": report["kind"],
        "tag": report.get("tag") or "", "test_dir": report["test_dir"], "n_images": m["n_images"],
        "accuracy": m["accuracy"], "accuracy_ci_low": m["accuracy_ci95"][0], "accuracy_ci_high": m["accuracy_ci95"][1],
        "balanced_accuracy": m["balanced_accuracy"], "macro_f1": m["macro_f1"],
        "macro_f1_ci_low": m["macro_f1_ci95"][0], "macro_f1_ci_high": m["macro_f1_ci95"][1],
        "positive_class": p.get("name", ""), "pos_precision": p.get("precision", ""), "pos_recall": p.get("recall", ""),
        "pos_recall_ci_low": p.get("recall_ci95", ("", ""))[0], "pos_recall_ci_high": p.get("recall_ci95", ("", ""))[1],
        "false_alarm_rate": p.get("false_alarm_rate", ""), "roc_auc": p.get("roc_auc", ""), "pr_auc": p.get("pr_auc", ""),
        "size_kb": m.get("size_kb", ""), "params": m.get("params", ""),
        "agreement_with_keras": m.get("agreement_with_keras", ""),
        "quantization_accuracy_drop": m.get("quantization_accuracy_drop", ""),
    }
    csv_path = out_dir / "results.csv"
    new_file = not csv_path.exists()
    with open(csv_path, "a", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=CSV_COLUMNS)
        if new_file:
            writer.writeheader()
        writer.writerow({k: (f"{v:.6f}" if isinstance(v, float) else v) for k, v in row.items()})
    print(f"\nWrote {json_path} and appended to {csv_path}")


# ---------------------------------------------------------------- entry points

def evaluate_models(model_paths: list[Path], test_dir: Path, image_size: int,
                    positive_class: str | None = "fire", out_dir: Path | None = EVALUATIONS_DIR,
                    tag: str | None = None) -> dict[Path, dict]:
    """
    Evaluate several models (.keras and/or .tflite) on the same test set.
    If X.keras is evaluated together with X.tflite or X_int8.tflite, the tflite
    report additionally contains the agreement with / accuracy drop from it.
    out_dir: where to write JSON reports and results.csv (None = print only)
    returns: model path -> metrics
    """
    images, labels, class_names = load.load_test_images(test_dir, image_size)
    images, labels = np.stack(images), np.asarray(labels)
    print(f"Evaluating on {len(images)} test images from {test_dir}, classes: {class_names}")

    results = {}
    keras_refs = {}  # keras model stem -> (predictions, accuracy)
    # keras models first so the tflite models can be compared against them
    for path in sorted(model_paths, key=lambda p: p.suffix != ".keras"):
        params = None
        if path.suffix == ".keras":
            scores, params = predict_keras(path, images)
        elif path.suffix == ".tflite":
            scores = predict_tflite(path, images)
        else:
            raise SystemExit(f"Unsupported model type: {path}")

        m = compute_metrics(labels, scores, class_names, positive_class)
        m["size_kb"] = path.stat().st_size / 1024
        if params is not None:
            m["params"] = int(params)

        predictions = scores.argmax(axis=1)
        keras_ref = keras_refs.get(path.stem.removesuffix("_int8"))  # X.tflite / X_int8.tflite <-> X.keras
        if path.suffix == ".keras":
            keras_refs[path.stem] = (predictions, m["accuracy"])
        elif keras_ref is not None:
            m["agreement_with_keras"] = float(np.mean(predictions == keras_ref[0]))
            m["quantization_accuracy_drop"] = float(m["accuracy"] - keras_ref[1])

        print_report(path.name, m, class_names)
        if out_dir is not None:
            save_results(out_dir, {
                "timestamp": datetime.now().isoformat(timespec="seconds"),
                "model": path.stem, "kind": path.suffix.lstrip("."), "path": str(path), "tag": tag,
                "test_dir": str(test_dir), "image_size": image_size, "class_names": class_names,
                "metrics": m,
            })
        results[path] = m
    return results


def evaluate_tflite_model(tfLiteModel: Path, test_dir: Path, image_size: int, **kwargs) -> dict:
    """Backwards-compatible wrapper: evaluate a single tflite model."""
    return evaluate_models([tfLiteModel], test_dir, image_size, **kwargs)[tfLiteModel]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--model", type=Path, nargs="+", required=True, help="One or more .keras / .tflite models")
    parser.add_argument("--test-dir", type=Path, default=cli_defaults.TEST_DIR, help="Path to test data directory")
    parser.add_argument("--image-size", type=int, default=cli_defaults.IMAGE_SIZE, help="Image size")
    parser.add_argument("--positive-class", default="fire", help="Class treated as 'detection' for recall/false alarms/AUC")
    parser.add_argument("--out-dir", type=Path, default=EVALUATIONS_DIR, help="Where JSON reports and results.csv go")
    parser.add_argument("--no-save", action="store_true", help="Only print, don't write results")
    parser.add_argument("--tag", help="Free-text note stored with the results (e.g. 'lr=1e-3, no aug')")
    args = parser.parse_args()

    if not args.test_dir.is_dir():
        raise SystemExit(f"{args.test_dir} not found -- split the dataset first.")

    evaluate_models(args.model, args.test_dir, args.image_size, args.positive_class,
                    None if args.no_save else args.out_dir, args.tag)


if __name__ == "__main__":
    main()
