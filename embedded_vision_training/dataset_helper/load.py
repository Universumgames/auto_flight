from pathlib import Path

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

    train_ds = load_dataset(ds_dir / "train", image_size, batch_size, augmentation)
    val_ds = load_dataset(ds_dir / "val", image_size, batch_size, None)
    class_names = train_ds.class_names

    return train_ds, val_ds, class_names

def load_dataset(ds_dir: Path, image_size: int, batch_size: int, augmentation: Sequential | None) -> tf.data.Dataset:
    ds = keras.utils.image_dataset_from_directory(
        ds_dir,
        image_size=(image_size, image_size),
        batch_size=batch_size,
        label_mode="int"
    )
    if augmentation is not None:
        ds = ds.map(lambda  x,y: (augmentation(x, training=True), y))
    ds.prefetch(tf.data.AUTOTUNE)
    return ds

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


def representative_dataset(train_dir: Path, image_size: int, num_samples: int):
    """
    Get a representative dataset for training or quantisation
    train_dir: path to the training data directory
    image_size: the image size
    num_samples: the number of samples
    """
    train_ds = tf.keras.utils.image_dataset_from_directory(
        train_dir,
        image_size=(image_size, image_size),
        batch_size=1,
        shuffle=True,
    )

    for images, _ in train_ds.unbatch().batch(1).take(num_samples):
        yield [tf.cast(images, tf.float32)]
