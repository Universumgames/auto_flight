import { CHANNEL_ORDER, CHANNEL_TYPE, VALUE_RANGE } from './config.js';

function clamp(value, min, max) {
  return Math.min(max, Math.max(min, value));
}

function normalize(channelName, rawValue) {
  const clamped = clamp(rawValue, VALUE_RANGE.min, VALUE_RANGE.max);
  const type = CHANNEL_TYPE[channelName];
  if (type === 'unipolar') {
    return (clamped - VALUE_RANGE.min) / (VALUE_RANGE.max - VALUE_RANGE.min);
  }
  // bipolar
  const half = (VALUE_RANGE.max - VALUE_RANGE.min) / 2;
  return (clamped - VALUE_RANGE.center) / half;
}

// Parses a single CSV line (e.g. "0,50,0,-40") into normalized
// channel values, e.g. { motor: 0.5, roll: 0.2, yaw: 0.0, pitch: -0.2 }.
// Returns null for malformed lines (wrong field count, non-numeric values)
// so callers can simply drop the frame and keep the last good one.
export function parseLine(line) {
  const trimmed = line.trim();
  if (!trimmed) return null;

  const rawFields = trimmed.split(',').map(Number);
  if (rawFields.length !== CHANNEL_ORDER.length) return null;
  if (rawFields.some(Number.isNaN)) return null;

  const values = {};
  CHANNEL_ORDER.forEach((channelName, i) => {
    values[channelName] = normalize(channelName, rawFields[i]);
  });
  return values;
}

// Incrementally buffers raw text chunks and yields complete lines.
// Handles CSV lines that straddle chunk boundaries.
export class LineBuffer {
  constructor() {
    this.buffer = '';
  }

  // Push a chunk of decoded text, returns an array of complete lines (without newline).
  push(chunk) {
    this.buffer += chunk;
    const lines = [];
    let newlineIndex;
    while ((newlineIndex = this.buffer.indexOf('\n')) !== -1) {
      lines.push(this.buffer.slice(0, newlineIndex));
      this.buffer = this.buffer.slice(newlineIndex + 1);
    }
    return lines;
  }
}
