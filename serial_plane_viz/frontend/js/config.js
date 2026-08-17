// Reorder this array to match your transmitter/receiver's actual wire order.
// Nothing else in parser.js / plane.js needs to change.
export const CHANNEL_ORDER = ['yaw', 'pitch', 'motor', 'roll', ];

// Raw incoming value range. Matches motor_controller's raw command range
// (int8_t, -100..100) received over I2C before its mapToServo() converts to
// actual PWM microseconds. 0 = neutral/center.
export const VALUE_RANGE = { min: -100, max: 100, center: 0 };

// 'unipolar': VALUE_RANGE.min..max -> 0..1 (throttle-like, always positive)
// 'bipolar':  VALUE_RANGE.min..max -> -1..+1, centered at VALUE_RANGE.center (flight-surface-like)
export const CHANNEL_TYPE = {
  motor: 'unipolar',
  roll: 'bipolar',
  yaw: 'bipolar',
  pitch: 'bipolar',
};

// Default WebSocket bridge address (see bridge/serial_to_ws_bridge.py).
export const DEFAULT_WS_URL = 'ws://localhost:8765';

// Default Web Serial baud rate.
export const DEFAULT_BAUD_RATE = 115200;

// How far each control-surface flap (ailerons, elevator, rudder) extends
// backward from its hinge line, in world units. Purely visual — increase
// for more visible flaps.
export const FLAP_DEPTH = 0.12;

// Set a surface to true if its real servo(s) are mounted/geared so the
// surface moves opposite to what this viz assumes by default. This flips
// the whole pair together (e.g. both ailerons), not just one side, so their
// differential motion relative to each other is preserved.
export const SERVO_FLIP = {
  aileron: true,
  elevator: false,
  rudder: false,
};
