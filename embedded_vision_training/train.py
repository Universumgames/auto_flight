"""Train the fire/smoke CNN on data/{train,val}/<class>/*.jpg."""

import argparse

import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers

from config import IMAGE_SIZE, MODEL_KERAS_PATH, LABELS_PATH, MODELS_DIR, TRAIN_DIR, VAL_DIR
from model import build_model

AUGMENTATION = keras.Sequential(
    [
        layers.RandomFlip("horizontal"),
        layers.RandomRotation(0.08),
        layers.RandomZoom(0.15),
        layers.RandomContrast(0.15),
    ],
    name="augmentation",
)


def load_datasets(batch_size: int):
    train_ds = keras.utils.image_dataset_from_directory(
        TRAIN_DIR,
        image_size=(IMAGE_SIZE, IMAGE_SIZE),
        batch_size=batch_size,
        label_mode="int",
    )
    val_ds = keras.utils.image_dataset_from_directory(
        VAL_DIR,
        image_size=(IMAGE_SIZE, IMAGE_SIZE),
        batch_size=batch_size,
        label_mode="int",
    )
    class_names = train_ds.class_names

    train_ds = train_ds.map(lambda x, y: (AUGMENTATION(x, training=True), y))
    train_ds = train_ds.prefetch(tf.data.AUTOTUNE)
    val_ds = val_ds.prefetch(tf.data.AUTOTUNE)
    return train_ds, val_ds, class_names


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--epochs", type=int, default=40)
    parser.add_argument("--batch-size", type=int, default=32)
    parser.add_argument("--learning-rate", type=float, default=1e-3)
    args = parser.parse_args()

    if not TRAIN_DIR.is_dir() or not VAL_DIR.is_dir():
        raise SystemExit("data/train and data/val not found -- run prepare_dataset.py first.")

    MODELS_DIR.mkdir(parents=True, exist_ok=True)

    train_ds, val_ds, class_names = load_datasets(args.batch_size)
    print(f"Classes ({len(class_names)}): {class_names}")

    model = build_model(num_classes=len(class_names))
    model.compile(
        optimizer=keras.optimizers.Adam(args.learning_rate),
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

    model.fit(train_ds, validation_data=val_ds, epochs=args.epochs, callbacks=callbacks)

    # ModelCheckpoint already wrote the best model to MODEL_KERAS_PATH, but
    # make sure it exists even if val_accuracy never improved from -inf.
    if not MODEL_KERAS_PATH.exists():
        model.save(MODEL_KERAS_PATH)

    LABELS_PATH.write_text("\n".join(class_names) + "\n")
    print(f"\nSaved model to {MODEL_KERAS_PATH}")
    print(f"Saved labels to {LABELS_PATH}")


if __name__ == "__main__":
    main()
