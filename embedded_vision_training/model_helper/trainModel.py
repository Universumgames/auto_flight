"""Train the fire/smoke CNN on data/{train,val}/<class>/*.jpg."""

import argparse
from pathlib import Path
from typing import Any

import keras
import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers

from config import IMAGE_SIZE, MODEL_KERAS_PATH, LABELS_PATH, MODELS_DIR, TRAIN_DIR, VAL_DIR, ROOT_DIR
from dataset_helper.load import load_datasets
from model_helper.modelCreator import create_raw_model

AUGMENTATION = keras.Sequential(
    [
        layers.RandomFlip("horizontal"),
        layers.RandomRotation(0.08),
        layers.RandomZoom(0.15),
        layers.RandomContrast(0.15),
    ],
    name="augmentation",
)

def train_new_model(model_name: str, train_ds: tf.data.Dataset, val_ds: tf.data.Dataset, class_names: list[str], learning_rate: float, epochs: int):
    """
    Train a new model
    model_name: name the model should be saved as
    train_ds: training dataset
    val_ds: validation dataset
    class_names: list of all classnames
    learning_rate: the learning rate
    epochs: number of epochs to run
    """
    model = create_raw_model(name=model_name, num_classes=len(class_names))
    model.compile(
        optimizer=keras.optimizers.Adam(learning_rate),
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    model.summary()

    callbacks = [
        keras.callbacks.ModelCheckpoint(
            str(MODEL_KERAS_PATH), monitor="val_accuracy", save_best_only=True, verbose=1
        ),
        keras.callbacks.EarlyStopping(monitor="val_accuracy", patience=8, restore_best_weights=True),
        keras.callbacks.ReduceLROnPlateau(monitor="val_loss", factor=0.5, patience=4),
    ]

    model.fit(train_ds, validation_data=val_ds, epochs=epochs, callbacks=callbacks)

    # ModelCheckpoint already wrote the best model to MODEL_KERAS_PATH, but
    # make sure it exists even if val_accuracy never improved from -inf.
    if not MODEL_KERAS_PATH.exists():
        model.save(MODEL_KERAS_PATH)

    LABELS_PATH.write_text("\n".join(class_names) + "\n")
    print(f"\nSaved model to {MODEL_KERAS_PATH}")
    print(f"Saved labels to {LABELS_PATH}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--epochs", type=int, default=40)
    parser.add_argument("--batch-size", type=int, default=32)
    parser.add_argument("--learning-rate", type=float, default=1e-3)
    args = parser.parse_args()

    if not TRAIN_DIR.is_dir() or not VAL_DIR.is_dir():
        raise SystemExit("data/train and data/val not found -- run prepare_dataset.py first.")

    MODELS_DIR.mkdir(parents=True, exist_ok=True)

    train_ds, val_ds, class_names = load_datasets(ROOT_DIR/"data", AUGMENTATION, IMAGE_SIZE, args.batch_size)
    print(f"Classes ({len(class_names)}): {class_names}")

    train_new_model("fire_smoke_cnn", train_ds, val_ds, class_names, args.learning_rate, args.epochs)


if __name__ == "__main__":
    main()
