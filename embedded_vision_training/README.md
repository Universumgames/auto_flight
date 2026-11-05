# Fire/Smoke CNN Training

Trains a small image classifier (fire / smoke / none) and exports it as an
int8-quantized TensorFlow Lite model, ready to embed into the
[`vision_controller`](../vision_controller) ESP-IDF project.

## Pipeline

```
data/raw/<class>/*.jpg
        │  prepare_dataset.py
        ▼
data/{train,val,test}/<class>/*.jpg
        │  train.py
        ▼
models/fire_smoke_cnn.keras, models/labels.txt
        │  model_helper/modelConverter.py
        ▼
models/fire_smoke_cnn.tflite
        │  evaluate.py (sanity check accuracy after quantization)
        │  export_c_array.py
        ▼
../vision_controller/main/model_data.{hpp,cpp}
```

## Setup

Requires [uv](https://docs.astral.sh/uv/). It installs the pinned Python
version (`.python-version`) and the locked dependencies (`uv.lock`):

```bash
uv sync        # or: make venv
```

Run scripts with `uv run <script>.py` (the `make` targets below do this for
you), or `source .venv/bin/activate` to use the environment directly.
Add dependencies with `uv add <package>`.

## 1. Get data

Populate `data/raw/<class>/*.jpg`, one folder per class, e.g.:

```
data/raw/fire/*.jpg
data/raw/smoke/*.jpg
data/raw/none/*.jpg
```

Folder names become the model's class labels (sorted alphabetically). You
need at least two classes, and "none" (negative/background images) matters
as much as the positive classes for a usable detector.

To get started quickly:

```bash
make download   # pulls a starter fire/non-fire dataset via kagglehub (see
                # download_dataset.py for details and caveats -- you'll need
                # a Kaggle account/API token, and it does NOT include smoke).
```

Add `data/raw/smoke/*.jpg` yourself (e.g. crop frames from a fire+smoke
object-detection dataset, or search Kaggle for a smoke-classification set)
if you want smoke as its own class rather than merging it into "fire".

## 2. Run the pipeline

```bash
make prepare   # splits data/raw into train/val/test (70/15/15)
make train     # trains the CNN, ~a few minutes on CPU for a small dataset
make convert   # int8-quantizes to .tflite
make evaluate  # accuracy/confusion matrix on the test set, post-quantization
make export    # writes model_data.hpp/.cpp into ../vision_controller/main/
```

or all at once (skipping download, assuming `data/raw/` is already populated):

```bash
make pipeline
```

## Model

`model.py` builds a small depthwise-separable CNN (same architectural
family as MobileNet / the TFLite Micro "person detection" reference model):
96x96x3 input, ~5 depthwise-separable blocks, global average pooling, dense
softmax head. This is deliberately small -- the ESP32 needs to fit the
whole interpreter, tensor arena, and activations in a few hundred KB of RAM,
not just the model weights. If you need more accuracy and have headroom
(e.g. an S3 board with PSRAM), widen the `filters` in `model.py` and/or
increase `IMAGE_SIZE` in `config.py`; both directly trade off against the
tensor arena size configured in `vision_controller/main/inference.cpp`.

## Why int8 quantization

TensorFlow Lite Micro on ESP32 needs a fully integer-quantized model (int8
weights, activations, input and output) -- there's no practical float32
inference path at any useful framerate without an FPU-heavy CPU. This
happens in `model_helper/modelConverter.py` via post-training quantization, using a
sample of the training set as the representative dataset TFLite needs to
compute per-tensor quantization ranges. `evaluate.py` re-checks accuracy
after quantization since int8 rounding can occasionally hurt a borderline
class more than others.
