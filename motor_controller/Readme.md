# Motor Controller

An Arduino Nano that sits between the RC receiver and the four control
servos/ESC of the plane. It normally takes its servo targets over I2C from
the flight controller (ESP32), but falls back to reading the SBUS RC
receiver directly if the pilot flips a manual-override switch, so the plane
stays flyable even if the flight controller crashes or loses power.

## Pinout

| Signal        | Pin |
|---------------|-----|
| SBUS Receiver | Rx  |
| I2C SDA       | A4  |
| I2C SCL       | A5  |
| Servo 1       | D9  |
| Servo 2       | D10 |
| Servo 3       | D11 |
| Servo 4       | D12 |

Servo 1–4 map to aileron differential, pitch, thrust and rudder respectively
(the same order the flight controller sends them in, see
[I2C protocol](#i2c-protocol)).

## Inner working / mapping

- All RC channels and the I2C payload use a normalized `[-100, 100]` range,
  where `0` is center/neutral.
- `mapToServo()` linearly maps that range to the standard `[1000, 2000] µs`
  RC PWM pulse width (`1500 µs` = center), clamping out-of-range input first.
- **Manual override**: SBUS channel 5 is read every loop; if its normalized
  value is `> 50`, the board drives the servos directly from SBUS channels
  1–4 instead of the last values received over I2C. This is the safety
  fallback — flipping the corresponding switch on the transmitter takes the
  flight controller out of the loop entirely.
  - Note: the checked-in build currently defines `DEBUG_PLANE`/`DEBUG_I2C`,
    which forces `manualOverride` to `false` in `loop()` regardless of the
    switch, and also skips `sbus.begin()` in favor of a `Serial` debug
    console. Remove those defines for a normal flight build.
- `loop()` just re-evaluates and re-writes all four servos every iteration —
  there is no fixed frame rate; a servo simply holds its last commanded
  position until the next write.

## I2C protocol

The Arduino is an **I2C slave** at address **`0x42`** (`Wire.begin(0x42)`,
100 kHz bus clock as configured on this side — actual clock is dictated by
the master). There is no register addressing; it is a plain fixed-size
write / single-byte read protocol.

### Write (master → Arduino): set servo targets

Master must write **exactly 4 bytes**, each a signed `int8_t` in
`[-100, 100]`:

| Byte | Meaning                        |
|------|--------------------------------|
| 0    | Servo 1 (aileron diff) target  |
| 1    | Servo 2 (pitch) target         |
| 2    | Servo 3 (thrust) target        |
| 3    | Servo 4 (rudder) target        |

Effect: replaces all four stored target values at once (there is no partial
update — every write must supply all 4 bytes). These values are applied to
the servos on the next `loop()` iteration, but only while manual override is
*not* active; if the RC override switch is engaged, I2C writes are still
accepted and stored but have no visible effect until override is released.

If fewer than 4 bytes are received, the whole packet is discarded and the
previously stored values are kept unchanged.

### Read (master ← Arduino): status / liveness

A master read returns a single byte: `1` if manual override is currently
active (the board is ignoring I2C targets and flying off the RC receiver),
`0` otherwise. Since any successful read implies the slave acked its
address, this same read also doubles as a "is the controller connected and
responding" check regardless of the byte's value.