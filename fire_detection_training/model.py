"""Tiny depthwise-separable CNN, sized to fit an ESP32 tensor arena.

Architecture is a scaled-down MobileNet (same family as the TFLite Micro
"person detection" reference model), which is the standard shape for image
classifiers that need to run on microcontrollers.
"""

from tensorflow import keras
from tensorflow.keras import layers

from config import CHANNELS, IMAGE_SIZE


def _depthwise_separable_block(x, filters: int, strides: int):
    x = layers.DepthwiseConv2D(3, strides=strides, padding="same", use_bias=False)(x)
    x = layers.BatchNormalization()(x)
    x = layers.ReLU()(x)
    x = layers.Conv2D(filters, 1, padding="same", use_bias=False)(x)
    x = layers.BatchNormalization()(x)
    x = layers.ReLU()(x)
    return x


def build_model(num_classes: int, image_size: int = IMAGE_SIZE) -> keras.Model:
    inputs = keras.Input(shape=(image_size, image_size, CHANNELS), name="image")

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

    return keras.Model(inputs, outputs, name="fire_smoke_cnn")


if __name__ == "__main__":
    build_model(num_classes=3).summary()
