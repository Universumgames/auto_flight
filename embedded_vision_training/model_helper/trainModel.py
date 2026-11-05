"""Train the fire/smoke CNN on data/{train,val}/<class>/*.jpg."""

import argparse
from pathlib import Path

import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import Model
from tensorflow.keras.callbacks import History

import cli_defaults
import constants
from dataset_helper.load import load_datasets
from model_helper.commons import AUGMENTATION
from model_helper.modelCreator import create_raw_model
from model_helper.historyExporter import save_history


def model_filenames(output_dir: Path, model_name: str) -> dict[str, Path]:
    model_filename = output_dir / (model_name + ".keras")
    labels_filename = output_dir / (model_name + ".labels.txt")
    tflite_filename = output_dir / (model_name + ".tflite")
    data = {
        "model": model_filename,
        "labels": labels_filename,
        "tflite": tflite_filename
    }
    return data

def train_new_model(model_name: str, output_dir: Path, color_channels: int, image_size: int, train_ds: tf.data.Dataset, val_ds: tf.data.Dataset, class_names: list[str], learning_rate: float, epochs: int) -> tuple[Model, History]:
    """
    Train a new model
    model_name: name the model should be saved as
    train_ds: training dataset
    val_ds: validation dataset
    class_names: list of all classnames
    learning_rate: the learning rate
    epochs: number of epochs to run
    """
    model = create_raw_model(name=model_name, num_classes=len(class_names), color_channel_count=color_channels, image_size=image_size)
    filenames = model_filenames(output_dir, model_name)
    model_filename = filenames["model"]
    labels_filename = filenames["labels"]
    model.compile(
        optimizer=keras.optimizers.Adam(learning_rate),
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    model.summary()

    callbacks = [
        keras.callbacks.ModelCheckpoint(
            model_filename, monitor="val_accuracy", save_best_only=True, verbose=1
        ),
        keras.callbacks.EarlyStopping(monitor="val_accuracy", patience=8, restore_best_weights=True),
        keras.callbacks.ReduceLROnPlateau(monitor="val_loss", factor=0.5, patience=4),
    ]

    labels_filename.write_text("\n".join(class_names) + "\n")
    history = model.fit(train_ds, validation_data=val_ds, epochs=epochs, callbacks=callbacks)

    # ModelCheckpoint already wrote the best model to MODEL_KERAS_PATH, but
    # make sure it exists even if val_accuracy never improved from -inf.
    if not model_filename.exists():
        model.save(model_filename)

    print(f"\nSaved model to {model_filename}")
    print(f"Saved labels to {labels_filename}")
    return model, history


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--epochs", type=int, default=2)
    parser.add_argument("--batch-size", type=int, default=32)
    parser.add_argument("--learning-rate", type=float, default=1e-3)
    parser.add_argument("--split-dir", type=Path, default=cli_defaults.DATA_DIR, help="Path to directory containing split for train and validation data")
    parser.add_argument("--image-size", type=int, default=cli_defaults.IMAGE_SIZE)
    parser.add_argument("--channels", type=int, default=cli_defaults.CHANNELS)
    parser.add_argument("--out-dir", type=Path, default=constants.MODELS_DIR)
    parser.add_argument("--name", type=str, help="Model name", default="debug")
    args = parser.parse_args()

    if not args.split_dir.is_dir():
        raise SystemExit("data/train and data/val not found -- run prepare_dataset.py first.")

    args.out_dir.mkdir(parents=True, exist_ok=True)

    train_ds, val_ds, class_names = load_datasets(args.split_dir, AUGMENTATION, args.image_size, args.batch_size)
    print(f"Classes ({len(class_names)}): {class_names}")

    model, history = train_new_model(args.name, args.out_dir, args.channels, args.image_size, train_ds, val_ds, class_names, args.learning_rate, args.epochs)
    save_history(history, args.name)

if __name__ == "__main__":
    main()
