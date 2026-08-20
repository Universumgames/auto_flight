# fire_detection_esp

ESP-IDF firmware that runs the CNN trained in
[`fire_detection_training`](../fire_detection_training) on an ESP32, via
[TensorFlow Lite Micro](https://github.com/espressif/esp-tflite-micro)
(pulled in as a managed component -- see `main/idf_component.yml`).

Built and verified against ESP-IDF v5.4, target `esp32s3` (also builds
against plain `esp32`, just with less RAM headroom for the tensor arena --
see `main/inference.cpp`).

## Architecture

Two independent FreeRTOS tasks, started from `app_main()` in `main.cpp`:

- **`inference_task`** (`main.cpp`, pinned to core 1): grabs a frame from
  `image_provider.cpp`, runs it through `fire_inference_run()`
  (`inference.cpp`), logs the predicted class + confidence, repeats every 2s.
- **`i2c_task`** (`i2c_task.cpp`, pinned to core 0): a template for any other
  sensor work you want running concurrently -- owns the I2C bus via
  ESP-IDF's `driver/i2c_master.h`, polls a (placeholder) register, and
  exposes the latest reading through `i2c_task_get_latest()` behind a
  mutex. Point `SENSOR_I2C_ADDR`/`read_register()` at your actual sensor
  and adjust the protocol as needed.

These two tasks don't share any state, so there's no locking between them.
If you do need to pass data between tasks (e.g. feed a barometer reading
into a decision alongside the CNN output), either:

- a mutex-protected shared variable, like `i2c_task_get_latest()` already
  does, for "latest value" semantics, or
- a FreeRTOS queue (`xQueueCreate`/`xQueueSend`/`xQueueReceive`) if you need
  to hand off discrete events without dropping any.

This is the standard ESP-IDF concurrency model: FreeRTOS tasks, not OS
threads. If you'd rather write standard C++ (`std::thread`, `std::mutex`)
instead of the FreeRTOS primitives used here, ESP-IDF's `pthread` component
maps those onto FreeRTOS tasks for you underneath -- same mechanism, different
API surface. Either way, `xTaskCreatePinnedToCore`'s core argument (0 or 1)
and priority argument are how you control where and how eagerly a task runs;
see the comments in `main.cpp` and `i2c_task.cpp` for the reasoning behind the
current choices.

## Getting a model in

`main/model_data.hpp`/`.cpp` start out as a placeholder (an empty model,
`g_model_data_len == 0`) so the project builds out of the box. Before it
does anything useful:

1. Train a model: see [`../fire_detection_training/README.md`](../fire_detection_training/README.md).
2. Run `make export` there (or `python export_c_array.py`), which
   overwrites `main/model_data.hpp`/`.cpp` with your trained model as a
   C++ array, plus the class labels.
3. Rebuild this project.

`fire_inference_init()` (`main/inference.cpp`) checks for the placeholder
and logs a clear error instead of crashing if you forget step 1-2.

### If you change the model architecture

`inference.cpp` registers a fixed, minimal set of TFLM ops via
`MicroMutableOpResolver` (not `AllOpsResolver` -- that class doesn't exist
in the currently vendored esp-tflite-micro). If you change
`fire_detection_training/model.py`'s architecture, the op list can change
too, and you'll get an "Op not found" runtime error rather than a build
error. Re-check the actual op list with:

```python
import tensorflow as tf
interp = tf.lite.Interpreter(model_path="models/fire_smoke_cnn_int8.tflite")
interp.allocate_tensors()
print(sorted({d['op_name'] for d in interp._get_ops_details()}))
```

and update the `resolver.Add*()` calls (and the `MicroMutableOpResolver<N>`
count) in `inference.cpp` to match.

## Camera

No camera has been wired in (this was left as a deliberate open choice --
see `image_provider.cpp`). Right now `image_provider_get_frame()` fills the
buffer with a synthetic test pattern, which is enough to verify the whole
pipeline (build, flash, run inference, see varying output) without any
camera hardware.

To add a real camera: bring in the `espressif/esp32-camera` managed
component (add it to `main/idf_component.yml`), initialize it for your
specific board's pin mapping, and replace the body of
`image_provider_get_frame()` with `esp_camera_fb_get()` + a resize/convert
step down to the model's expected size (see the TODO comment in
`image_provider.cpp` for specifics). The model's expected input
width/height is available at runtime via `fire_inference_get_input_size()`.

## Build & flash

```bash
. $IDF_PATH/export.sh        # or: get_idf, depending on your shell setup
idf.py set-target esp32s3    # or esp32
idf.py build
idf.py -p PORT flash monitor
```

`sdkconfig.defaults` sets a custom, larger partition table
(`partitions.csv`) since TFLM's kernels + the embedded model push the app
image past ESP-IDF's default ~1MB factory partition. Requires 4MB+ flash.

## Known gaps / next steps

- No camera (see above) -- `image_provider.cpp` is a synthetic stand-in.
- `i2c_task.cpp` talks to a placeholder device address/register, not a real
  sensor.
- The tensor arena (`kTensorArenaSize` in `inference.cpp`) is sized for the
  default tiny model; if you grow the model or bump `IMAGE_SIZE` in
  `fire_detection_training/config.py`, watch for `AllocateTensors()`
  failures and grow the arena (or move it into PSRAM on an S3 board).
- No test coverage on the firmware side, matching the rest of this
  workspace's ESP-IDF projects (see the top-level README's "Known
  limitations" section) -- verification here has been build/flash/log-level
  only.
