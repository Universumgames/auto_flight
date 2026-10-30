# Running a TFLite Model on an ESP32-S3 with `esp-tflite-micro`

A practical setup guide for an ESP-IDF (C++) project. It covers adding the component, converting and embedding your model, loading it, running inference, and turning the raw output into **labels + confidence**. It also shows how to feed camera frames in.

---

## 0. The big picture

```
 PC (Python)                                   ESP32 (C++)
 ─────────────                                 ──────────────────────────────────────────
 Keras model                                   g_model[]  (byte array in flash)
   │  TFLiteConverter (full int8)                 │  tflite::GetModel()
   ▼                                              ▼
 model.tflite ──► model_data.cc ───────────►   MicroMutableOpResolver  (which ops exist)
                  (C array)                    tensor_arena            (RAM for all tensors)
                                                  │  MicroInterpreter + AllocateTensors()
                                                  ▼
                                               input tensor  ◄── quantize your pixels
                                                  │  Invoke()
                                                  ▼
                                               output tensor ──► dequantize ──► label + confidence
```

TFLite Micro has **no file system, no dynamic op loading and no labels**. That means:

* The model is compiled into the firmware as a byte array.
* You list every op the model uses yourself (the *op resolver*).
* You give it one fixed block of RAM (the *tensor arena*).
* Labels are a plain `const char*` array in your code, in the same order as in training.

---

## 1. Project setup

### 1.1 Add the component

From your project root:

```bash
idf.py add-dependency "espressif/esp-tflite-micro^1.3.3"
idf.py add-dependency "espressif/esp32-camera"      # only if you use the camera
```

This creates or updates `main/idf_component.yml`:

```yaml
dependencies:
  idf: ">=5.1"
  espressif/esp-tflite-micro: "^1.3.3"
  espressif/esp32-camera: "*"
```

The component is downloaded into `managed_components/` on the next build. **ESP-NN** (Espressif's optimized int8 kernels) is included and enabled automatically, so you don't need to do anything for it.

> If you put the inference code in **your own component** instead of `main`, add
> `REQUIRES espressif__esp-tflite-micro` to that component's `idf_component_register(...)`.
> The managed component name uses `__` instead of `/`.

### 1.2 Project layout used in this guide

```
vision_controller/
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv
└── main/
    ├── CMakeLists.txt
    ├── idf_component.yml
    ├── main.cc
    ├── inference.h / inference.cc   ← all TFLM code lives here
    ├── labels.h
    └── model_data.h / model_data.cc ← generated from model.tflite
```

`main/CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "main.cc" "inference.cc" "model_data.cc"
    INCLUDE_DIRS "."
)
```

### 1.3 `sdkconfig.defaults`

Delete `sdkconfig` once after creating this file so the defaults get applied.

```ini
# Speed
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
CONFIG_COMPILER_OPTIMIZATION_PERF=y

# TFLM uses quite a bit of stack in AllocateTensors()/Invoke()
CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384

# PSRAM (needed for camera frame buffers and for big arenas)
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y          # XIAO ESP32S3 Sense / N8R8 / N16R8 modules = octal.
                                  # Use CONFIG_SPIRAM_MODE_QUAD=y for quad-PSRAM modules
CONFIG_SPIRAM_SPEED_80M=y

# Flash + bigger app partition (models make the binary large)
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y  # set to your module's flash size
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
```

`partitions.csv`:

```csv
# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x6000
phy_init, data, phy,     0xf000,  0x1000
factory,  app,  factory, 0x10000, 3M
```

---

## 2. Preparing the model (Python side)

### 2.1 Convert to a **fully int8-quantized** `.tflite`

This matters a lot. ESP-NN only accelerates **int8** ops. A float model runs on the slow reference kernels and is often 20–40× slower.

```python
import tensorflow as tf

model = tf.keras.models.load_model("fire_model.keras")

def representative_dataset():
    # ~100–300 REAL training images, preprocessed exactly like during training
    for images, _ in train_ds.unbatch().batch(1).take(200):
        yield [tf.cast(images, tf.float32)]

converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.representative_dataset = representative_dataset
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter.inference_input_type  = tf.int8     # input tensor is int8
converter.inference_output_type = tf.int8     # output tensor is int8

tflite_model = converter.convert()
open("fire_model.tflite", "wb").write(tflite_model)
```

### 2.2 Find out which ops (and quantization params) the model uses

You need the op list for the resolver in step 4.

```python
tf.lite.experimental.Analyzer.analyze(model_path="fire_model.tflite")
```

Or open the file in **[Netron](https://netron.app)**. It shows every op, the input/output shapes, and the `scale` / `zero_point` of each tensor.

Also print the I/O details. They help with debugging later:

```python
interp = tf.lite.Interpreter(model_path="fire_model.tflite")
print(interp.get_input_details()[0])   # shape, dtype, quantization (scale, zero_point)
print(interp.get_output_details()[0])
```

### 2.3 Write down your label order

TFLM doesn't know your class names. The order is the order used in training. With `image_dataset_from_directory` that's **alphabetical folder order**:

```python
print(train_ds.class_names)   # e.g. ['fire', 'no_fire', 'smoke']
```

---

## 3. Embedding the model in the firmware

The model must be a byte array that is **aligned** (16 bytes is safe) and `const`, so it stays in flash and doesn't use RAM.

Use this small script instead of hand-editing `xxd` output (`tools/tflite_to_c.py`):

```python
#!/usr/bin/env python3
# usage: python tools/tflite_to_c.py fire_model.tflite main/model_data
import sys, pathlib

src, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
data = src.read_bytes()

out.with_suffix(".h").write_text(
    "#pragma once\n#include <cstdint>\n"
    "extern const unsigned char g_model[];\n"
    "extern const unsigned int g_model_len;\n"
)

lines = [", ".join(f"0x{b:02x}" for b in data[i:i + 12]) for i in range(0, len(data), 12)]
out.with_suffix(".cc").write_text(
    '#include "model_data.h"\n\n'
    f"// generated from {src.name} ({len(data)} bytes)\n"
    "alignas(16) const unsigned char g_model[] = {\n  "
    + ",\n  ".join(lines)
    + "\n};\n"
    f"const unsigned int g_model_len = {len(data)};\n"
)
print(f"wrote {out}.h/.cc ({len(data)} bytes)")
```

Run it again every time you retrain.

---

## 4. Loading the model and setting up the interpreter

### `main/labels.h`

```cpp
#pragma once
// MUST match train_ds.class_names order
static const char* const kLabels[] = { "fire", "no_fire", "smoke" };
static constexpr int kNumLabels = sizeof(kLabels) / sizeof(kLabels[0]);
```

### `main/inference.h`

```cpp
#pragma once
#include <cstdint>
#include <cstddef>
#include "tensorflow/lite/c/common.h"

struct Prediction {
    int         index;       // class index
    const char* label;       // class name
    float       confidence;  // 0..1
};

bool         inference_init();
TfLiteTensor* inference_input();             // fill this before inference_run()
bool         inference_run(Prediction* top, float* all_scores /* nullable, size kNumLabels */);

// Helper: float value -> quantized int8 using the input tensor's params
int8_t       quantize_input(float x);
```

### `main/inference.cc`

```cpp
#include "inference.h"
#include "labels.h"
#include "model_data.h"

#include <cmath>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

static const char* TAG = "inference";

namespace {
// Start big, then shrink it using the "arena used" log line below (+ ~10% margin).
constexpr size_t kArenaSize = 300 * 1024;

const tflite::Model*      model       = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor*             input       = nullptr;
TfLiteTensor*             output      = nullptr;
uint8_t*                  arena       = nullptr;
}  // namespace

bool inference_init() {
    tflite::InitializeTarget();

    // 1) Map the model (no copy, it's read straight from flash)
    model = tflite::GetModel(g_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(TAG, "Schema version %lu != supported %d",
                 (unsigned long)model->version(), TFLITE_SCHEMA_VERSION);
        return false;
    }

    // 2) Register EXACTLY the ops your model uses (see step 2.2).
    //    The template number must be >= the number of Add*() calls.
    static tflite::MicroMutableOpResolver<12> resolver;
    resolver.AddConv2D();
    resolver.AddDepthwiseConv2D();
    resolver.AddMaxPool2D();
    resolver.AddAveragePool2D();
    resolver.AddMean();            // GlobalAveragePooling2D usually becomes MEAN
    resolver.AddFullyConnected();
    resolver.AddReshape();
    resolver.AddSoftmax();         // or AddLogistic() for a single sigmoid output
    resolver.AddAdd();             // residual connections (MobileNetV2 etc.)
    resolver.AddPad();
    resolver.AddQuantize();
    resolver.AddDequantize();

    // 3) Tensor arena: internal RAM is fastest; fall back to PSRAM if it doesn't fit
    arena = (uint8_t*)heap_caps_aligned_alloc(16, kArenaSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!arena) {
        ESP_LOGW(TAG, "Arena not in internal RAM -> using PSRAM (slower)");
        arena = (uint8_t*)heap_caps_aligned_alloc(16, kArenaSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!arena) { ESP_LOGE(TAG, "Arena alloc failed"); return false; }

    // 4) Build the interpreter (must outlive this function -> static)
    static tflite::MicroInterpreter static_interpreter(model, resolver, arena, kArenaSize);
    interpreter = &static_interpreter;

    // 5) Plan memory for all tensors
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        ESP_LOGE(TAG, "AllocateTensors() failed (arena too small or missing op, see log above)");
        return false;
    }
    ESP_LOGI(TAG, "Arena used: %u / %u bytes",
             (unsigned)interpreter->arena_used_bytes(), (unsigned)kArenaSize);

    input  = interpreter->input(0);
    output = interpreter->output(0);

    // 6) Print what the model expects. Compare with Python's get_input_details()
    ESP_LOGI(TAG, "Input : type=%d dims=[%d,%d,%d,%d] scale=%f zp=%ld",
             input->type, input->dims->data[0], input->dims->data[1],
             input->dims->data[2], input->dims->data[3],
             input->params.scale, (long)input->params.zero_point);
    ESP_LOGI(TAG, "Output: type=%d elements=%d scale=%f zp=%ld",
             output->type, output->dims->data[output->dims->size - 1],
             output->params.scale, (long)output->params.zero_point);

    const int n_out = output->dims->data[output->dims->size - 1];
    if (n_out != kNumLabels && !(n_out == 1 && kNumLabels == 2)) {
        ESP_LOGE(TAG, "Model has %d outputs but %d labels", n_out, kNumLabels);
        return false;
    }
    return true;
}

TfLiteTensor* inference_input() { return input; }

int8_t quantize_input(float x) {
    // q = x / scale + zero_point, clamped to int8
    int32_t q = (int32_t)lroundf(x / input->params.scale) + input->params.zero_point;
    if (q < -128) q = -128;
    if (q >  127) q =  127;
    return (int8_t)q;
}

static float output_value(int i) {
    if (output->type == kTfLiteInt8)   // dequantize: (q - zero_point) * scale
        return (output->data.int8[i] - output->params.zero_point) * output->params.scale;
    if (output->type == kTfLiteFloat32)
        return output->data.f[i];
    return 0.f;
}

bool inference_run(Prediction* top, float* all_scores) {
    const int64_t t0 = esp_timer_get_time();
    if (interpreter->Invoke() != kTfLiteOk) {
        ESP_LOGE(TAG, "Invoke() failed");
        return false;
    }
    const int64_t dt = esp_timer_get_time() - t0;

    const int n_out = output->dims->data[output->dims->size - 1];

    if (n_out == 1) {
        // Binary model with a single sigmoid output: p = P(class 1)
        float p = output_value(0);
        if (all_scores) { all_scores[0] = 1.f - p; all_scores[1] = p; }
        top->index = p >= 0.5f ? 1 : 0;
        top->confidence = p >= 0.5f ? p : 1.f - p;
    } else {
        // Softmax: pick the max (argmax)
        int best = 0; float best_v = output_value(0);
        for (int i = 0; i < n_out; ++i) {
            float v = output_value(i);
            if (all_scores) all_scores[i] = v;
            if (v > best_v) { best_v = v; best = i; }
        }
        top->index = best;
        top->confidence = best_v;
    }
    top->label = kLabels[top->index];
    ESP_LOGD(TAG, "Invoke took %lld ms", dt / 1000);
    return true;
}
```

> **No softmax at the end of your model?** Then the outputs are *logits*, not probabilities.
> Either add a `Softmax` layer before converting (recommended), or apply softmax yourself
> to the dequantized values: `p_i = exp(x_i - max) / Σ exp(x_j - max)`.

---

## 5. Running inference

The loop is always the same: **fill input → `Invoke()` → read output**.

### 5.1 Filling the input tensor (quantization)

The input tensor is int8 with a `scale` and `zero_point`. You have to convert your values **exactly the way you preprocessed them in training**, then quantize:

```
real value x  (what the Keras model saw)  →  q = round(x / scale) + zero_point
```

Examples:

| Training preprocessing | `x` you compute on the ESP | Typical params | Shortcut |
|---|---|---|---|
| `Rescaling(1/255)` → 0..1 | `pixel / 255.0f` | scale ≈ 1/255, zp = -128 | `q = pixel - 128` |
| no rescaling, 0..255 | `pixel` | scale ≈ 1, zp = -128 | `q = pixel - 128` |
| MobileNet `preprocess_input` → -1..1 | `pixel / 127.5f - 1` | scale ≈ 1/127.5, zp ≈ 0 | `q = pixel - 128` |

In practice it is very often just `pixel - 128`. Still, **check the printed scale/zp**, or use `quantize_input()`, which is always correct.

Layout is **NHWC**, i.e. `[1][H][W][C]`, with channels interleaved (`R,G,B,R,G,B,…`):

```cpp
int8_t* in = inference_input()->data.int8;
// in[(y * W + x) * C + c] = quantize_input(value);
```

### 5.2 Minimal `main.cc` (static test image)

```cpp
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "inference.h"
#include "labels.h"

extern "C" void app_main(void) {
    if (!inference_init()) { ESP_LOGE("main", "init failed"); return; }

    TfLiteTensor* in = inference_input();
    const int H = in->dims->data[1], W = in->dims->data[2], C = in->dims->data[3];

    while (true) {
        // --- fill input (here: a dummy grey image) ---
        for (int i = 0; i < H * W * C; ++i)
            in->data.int8[i] = quantize_input(128 / 255.0f);

        // --- run ---
        Prediction top;
        float scores[kNumLabels];
        if (inference_run(&top, scores)) {
            ESP_LOGI("main", "=> %s (%.1f%%)", top.label, top.confidence * 100);
            for (int i = 0; i < kNumLabels; ++i)
                ESP_LOGI("main", "   %-10s %.3f", kLabels[i], scores[i]);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

---

## 6. Feeding camera frames (esp32-camera)

Let the camera give you a small RGB565 frame, then resize and quantize it **directly into the input tensor**. There's no extra buffer.

```cpp
#include "esp_camera.h"

// Call once at startup. Pins depend on the board (see the esp32-camera / board docs)
// camera_config_t cfg = { ... .pixel_format = PIXFORMAT_RGB565,
//                         .frame_size = FRAMESIZE_QVGA, .fb_count = 2,
//                         .fb_location = CAMERA_FB_IN_PSRAM,
//                         .grab_mode = CAMERA_GRAB_LATEST };
// esp_camera_init(&cfg);

// Nearest-neighbour resize RGB565 -> model input (RGB, int8 quantized)
static void frame_to_input(const camera_fb_t* fb, TfLiteTensor* in) {
    const int H = in->dims->data[1], W = in->dims->data[2], C = in->dims->data[3];
    int8_t* dst = in->data.int8;

    for (int y = 0; y < H; ++y) {
        const int sy = y * fb->height / H;
        for (int x = 0; x < W; ++x) {
            const int sx = x * fb->width / W;
            const uint8_t* p = fb->buf + (sy * fb->width + sx) * 2;
            const uint16_t px = (p[0] << 8) | p[1];          // esp32-camera RGB565 is big-endian
            const uint8_t r = ((px >> 11) & 0x1F) << 3;
            const uint8_t g = ((px >> 5)  & 0x3F) << 2;
            const uint8_t b = ( px        & 0x1F) << 3;

            int8_t* o = dst + (y * W + x) * C;
            if (C == 3) {
                o[0] = quantize_input(r / 255.0f);            // adapt to YOUR preprocessing
                o[1] = quantize_input(g / 255.0f);
                o[2] = quantize_input(b / 255.0f);
            } else {                                          // grayscale model
                o[0] = quantize_input((0.299f * r + 0.587f * g + 0.114f * b) / 255.0f);
            }
        }
    }
}

// in the loop:
camera_fb_t* fb = esp_camera_fb_get();
if (fb) {
    frame_to_input(fb, inference_input());
    esp_camera_fb_return(fb);              // return ASAP, the input tensor has its own copy
    Prediction top;
    inference_run(&top, nullptr);
    ESP_LOGI("main", "%s %.0f%%", top.label, top.confidence * 100);
}
```

Notes:

* If your training images were **center-cropped / square**, crop the frame to the same aspect ratio before resizing. Otherwise everything gets squashed and accuracy drops.
* Make sure the **channel order** matches training (RGB, not BGR) and that the color/white balance of the camera is roughly what your dataset looked like.
* For speed, precompute a 256-entry `int8_t lut[256]` with `quantize_input(v/255.f)` once and use `lut[r]` etc.

---

## 7. Tuning

### Arena size

1. Start with a generous `kArenaSize` (e.g. 300 KB).
2. Flash, then read `Arena used: N / ...` in the log.
3. Set `kArenaSize ≈ N * 1.1`, rounded up to 1 KB.

Rough rule: the arena needs the **largest pair of activation tensors alive at the same time**, not the model size. Large input resolutions (e.g. 224×224×3) and wide early conv layers drive it up.

### Where to put things

| Item | Location | Why |
|---|---|---|
| `g_model[]` | flash (`const`) | free, read via cache |
| tensor arena | internal RAM if it fits, else PSRAM | internal is noticeably faster |
| camera frame buffers | PSRAM | too big for internal RAM |

### Speed checklist

* Full int8 model (step 2.1). This is by far the biggest factor.
* `CONFIG_COMPILER_OPTIMIZATION_PERF=y`, CPU at 240 MHz.
* Small input resolution (96×96 or 128×128 is often enough for coarse classes like fire/smoke).
* MobileNet-style depthwise-separable convs are well optimized in ESP-NN.
* Measure with `esp_timer_get_time()` around `Invoke()` (already done in `inference_run`).

---

## 8. Verifying the ESP output against the PC

When the ESP results look wrong, run **the same `.tflite` on the same image on your PC** and compare the raw int8 outputs:

```python
import numpy as np, tensorflow as tf
from PIL import Image

interp = tf.lite.Interpreter(model_path="fire_model.tflite")
interp.allocate_tensors()
inp, out = interp.get_input_details()[0], interp.get_output_details()[0]
s_in, zp_in = inp["quantization"]
s_out, zp_out = out["quantization"]

h, w = inp["shape"][1:3]
img = np.asarray(Image.open("test.jpg").convert("RGB").resize((w, h)), np.float32) / 255.0  # same as training!
q = np.clip(np.round(img / s_in + zp_in), -128, 127).astype(np.int8)[None]

interp.set_tensor(inp["index"], q)
interp.invoke()
raw = interp.get_tensor(out["index"])[0]
print("raw int8:", raw)
print("dequant :", (raw.astype(np.float32) - zp_out) * s_out)
```

You can also dump `in->data.int8` from the ESP over serial and feed exactly those bytes to Python. If the outputs then match, the model is fine and the problem is in your preprocessing.

---

## 9. Troubleshooting

| Symptom / log message | Cause | Fix |
|---|---|---|
| `Didn't find op for builtin opcode 'XYZ'` | Op not registered | Add `resolver.AddXyz()` and raise the `<N>` template count |
| `Failed to resize buffer. Requested: X, available: Y, missing: Z` | Arena too small | Increase `kArenaSize` by at least `Z` |
| `Model provided is schema version N not equal to supported version 3` | Corrupt/wrong array | Regenerate `model_data.cc`; make sure the whole file was converted |
| `Type FLOAT32 (1) not supported` / op errors with int8 | Model not fully quantized | Use `TFLITE_BUILTINS_INT8` + `inference_input/output_type = tf.int8` |
| Crash / `LoadProhibited` inside `AllocateTensors()` or `Invoke()` | Stack overflow or unaligned model | Increase main task stack (or run inference in your own task with ≥16 KB); keep `alignas(16)` |
| Always predicts the same class | Preprocessing mismatch | Check normalization, RGB vs BGR, crop/resize, and quantization params (section 8) |
| Confidence never above ~0.5 / looks odd | Outputs are logits or you didn't dequantize | Add a softmax layer; use `(q - zp) * scale` |
| Very slow (seconds per frame) | Float model or arena in PSRAM | Full int8; try to fit the arena in internal RAM; reduce input size |
| `AddXyz()` returns error at init | Template count too small | `MicroMutableOpResolver<N>` with `N >=` number of ops |

---

## 10. Quick reference: the whole API you actually need

```cpp
tflite::InitializeTarget();
const tflite::Model* m = tflite::GetModel(g_model);         // map model
tflite::MicroMutableOpResolver<N> r; r.AddConv2D(); ...      // register ops
tflite::MicroInterpreter i(m, r, arena, arena_size);         // build interpreter
i.AllocateTensors();                                         // plan memory
i.arena_used_bytes();                                        // tune arena

TfLiteTensor* in  = i.input(0);   // in->data.int8, in->dims, in->params.{scale,zero_point}
TfLiteTensor* out = i.output(0);

/* fill in->data.int8 */            // q = round(x / scale) + zp
i.Invoke();                         // run
/* read out->data.int8 */           // x = (q - zp) * scale  → argmax → kLabels[idx]
```