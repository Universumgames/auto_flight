from tensorflow import keras
from tensorflow.keras import layers

AUGMENTATION = keras.Sequential(
    [
        layers.RandomFlip("horizontal"),
        layers.RandomRotation(0.08),
        layers.RandomZoom(0.15),
        layers.RandomContrast(0.15),
    ],
    name="augmentation",
)
