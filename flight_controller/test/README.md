# Native unit tests

This directory holds the native (host-machine) GoogleTest suite for `flight_controller` and the
`shared_components` it pulls in. It builds and runs on your dev machine (or in CI) without any
ESP-IDF toolchain, by compiling the same component sources against `cmake/idf_compat.cmake`, a
small mock of `idf_component_register()`.

## Running

```sh
cd flight_controller
cmake -S . -B build_native -DCMAKE_BUILD_TYPE=Debug
cmake --build build_native -j
./build_native/flight_controller_test
```

This is exactly what the `flight_controller_test` job in `.gitlab-ci.yml` runs. `build_native/` is
gitignored - safe to delete and regenerate any time.

To run a subset of tests, pass a GoogleTest filter: `./build_native/flight_controller_test --gtest_filter=SteeringLaw*`.

## Layout

Test files are grouped by the area they cover, mirroring `components/` and `shared_components/`:

- `route_planner/` - the route-planning/sweep-path algorithm
- `flight_control/` - the extracted flight-control-law math and moving averages
- `flight_data/` - geo helpers and the `Coordinate`/`ConnectionState`/`FlightState` JSON model
- `flight_com/` - the LoRa packet codec (encode/decode, malformed-input handling)
- `test.cpp` - the GoogleTest `main()`, shared by all of the above

New test files can go in a matching subfolder (or a new one) named `<area>_test.cpp` /
`<thing>_test.cpp` - `CMakeLists.txt` globs `test/**/*.*` recursively, so no CMake changes are
needed to pick up a new file.

## Why some components have almost no tests

Only a component's `NATIVE_BUILD` branch (see `if(NOT NATIVE_BUILD) ... else() ... endif()` in a
component's `CMakeLists.txt`) gets linked into this suite. Most components are hardware drivers
(I2C sensors, the LoRa radio, GPS UART, FreeRTOS tasks) with no meaningful native branch - there's
nothing pure to test host-side, and stubbing the driver calls would just be testing the stub.

The pattern used across this codebase for components that mix hardware and logic (`flight_data`,
`flight_com`, `flight_controller`) is to split the pure logic into its own translation unit with
no ESP-IDF/FreeRTOS/hardware-singleton dependencies, and register *only that file* in the
`NATIVE_BUILD` branch:

- `flight_data`: `types.cpp`/`serializer.cpp`/`geo_helper.cpp` are native-buildable;
  `FlightStorage.cpp` (FreeRTOS queues/tasks) is ESP-only.
- `flight_com`: `PacketCodec.cpp` (the `decodePacket` switch/validation logic) is native-buildable;
  `Flight_Communication.cpp` (drives the LoRa radio) is ESP-only.
- `flight_controller`: `SteeringLaw.cpp` (the PID/steering math, extracted out of
  `FlightControllerClass::steerToWaypoint`) is native-buildable; `FlightController.cpp` itself
  (orchestrates GPS/gyro/motor/etc singletons via FreeRTOS tasks) is ESP-only.

When adding new logic to a hardware-touching component, prefer writing it as a small pure
class/function in its own file and following this split from the start, rather than adding it
straight into the hardware-coupled `.cpp` where it can't be unit tested.

## Testing roadmap (not yet built)

- **On-target tests.** ESP-IDF has a standard per-component Unity test pattern: a `test/`
  directory per component gets built into a dedicated Unity-based test app
  (`idf.py create-test-component` scaffolds this), flashed and run on real hardware via
  `idf.py -p <PORT> flash monitor`. This is the right place for anything that needs real
  hardware/timing to verify - I2C sensor drivers (barometer, magnetometer, gyroscope), the LoRa
  radio, GPS UART parsing under real timing - none of which this native suite can meaningfully
  cover.
- **More pure-logic extraction candidates**, following the same split used above:
  `Gyroscope`'s `calibrateLevelOffset()`/complementary-filter math, `Magnetometer`'s heading
  calculation, `FlightStorage`'s pure state-transition helpers.
- **Integration tests.** Native "scenario" tests wiring `route_planner` + `FlightStorage` +
  `SteeringLaw` together end-to-end without real hardware; a hardware-in-the-loop runner for a
  self-hosted CI runner, if real boards are ever attached to CI.
- Native coverage reporting (gcov/lcov), if useful later.
