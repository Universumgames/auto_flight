from pathlib import Path
from typing import Any

from PIL.Image import Image
from tensorflow import keras
import tensorflow as tf
from tensorflow.python.keras import Sequential


def load_datasets(ds_dir: Path, augmentation: Sequential, image_size: int, batch_size: int) -> tuple[tf.data.Dataset, tf.data.Dataset, list[str]]:
    """
    Load training and validation dataset
    ds_dir: root dir where training and validation directories reside
    augmentation: augmentation pipeline definition
    image_size: size of images
    batch_size: the batch size used for training
    """
    train_ds = keras.utils.image_dataset_from_directory(
        ds_dir / "train",
        image_size=(image_size, image_size),
        batch_size=batch_size,
        label_mode="int",
    )
    val_ds = keras.utils.image_dataset_from_directory(
        ds_dir / "val",
        image_size=(image_size, image_size),
        batch_size=batch_size,
        label_mode="int",
    )
    class_names = train_ds.class_names

    train_ds = train_ds.map(lambda x, y: (augmentation(x, training=True), y))
    train_ds = train_ds.prefetch(tf.data.AUTOTUNE)
    val_ds = val_ds.prefetch(tf.data.AUTOTUNE)
    return train_ds, val_ds, class_names

def load_test_images(test_dir: Path, image_size: int) -> tuple[list[Image], list[int], list[str]]:
    """
    Load test image dataset
    test_dir: Directory with the test images
    image_size: the image size

    returns: images array, label indices, class_names
    """
    ds: tf.data.Dataset = tf.keras.utils.image_dataset_from_directory(
        test_dir,
        image_size=(image_size, image_size),
        batch_size=1,
        shuffle=False,
    )
    class_names = ds.class_names
    images, labels = [], []
    for image_batch, label_batch in ds:
        images.append(image_batch.numpy()[0])
        labels.append(int(label_batch.numpy()[0]))
    return images, labels, class_names
