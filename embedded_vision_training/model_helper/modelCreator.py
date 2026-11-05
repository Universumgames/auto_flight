from tensorflow import keras
from tensorflow.keras import layers

def _depthwise_separable_block(x, filters: int, strides: int):
    x = layers.DepthwiseConv2D(3, strides=strides, padding="same", use_bias=False)(x)
    x = layers.BatchNormalization()(x)
    x = layers.ReLU()(x)
    x = layers.Conv2D(filters, 1, padding="same", use_bias=False)(x)
    x = layers.BatchNormalization()(x)
    x = layers.ReLU()(x)
    return x

def create_raw_model(name: str, num_classes: int, image_size: int, color_channel_count: int) -> keras.Model:
    """
    name: the filename of the model
    num_classes: number of classes
    image_size: pixel size of the square input image
    color_channel_count: number of color channels (3 for RGB)
    """
    inputs = keras.Input(shape=(image_size, image_size, color_channel_count), name="image")

    # Rescale uint8 [0, 255] -> float32 [-1, 1]. This lives inside the graph
    # so the same preprocessing is baked into the exported TFLite model.
    x = layers.Rescaling(1.0 / 127.5, offset=-1.0)(inputs)

    x = layers.Conv2D(8, 3, strides=2, padding="same", use_bias=False)(x)
    x = layers.BatchNormalization()(x)
    x = layers.ReLU()(x)

    x = _depthwise_separable_block(x, 16, strides=1)
    x = _depthwise_separable_block(x, 32, strides=2)
    x = _depthwise_separable_block(x, 32, strides=1)
    x = _depthwise_separable_block(x, 64, strides=2)
    x = _depthwise_separable_block(x, 64, strides=1)

    x = layers.GlobalAveragePooling2D()(x)
    x = layers.Dropout(0.3)(x)
    outputs = layers.Dense(num_classes, activation="softmax", name="class")(x)

    model = keras.Model(inputs, outputs, name=name)
    return model

"""
import argparse
from pathlib import Path

from constants import MODELS_DIR


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-m", "--model", type=str, help="Name of the model")
    parser.add_argument("-c", "--classes", type=int)
    parser.add_argument("--channels", type=int, default=3, help="Number of color channels")
    parser.add_argument("--size", type=int, help="Image size")
    parser.add_argument("--output", type=Path, default=MODELS_DIR, help="Output directory of model")

    args = parser.parse_args()
    model = create_raw_model(name=args.model, num_classes=args.classes, image_size=args.size, color_channel_count=args.channels).summary()
    model.save(args.output / (args.model + ".keras"))

"""