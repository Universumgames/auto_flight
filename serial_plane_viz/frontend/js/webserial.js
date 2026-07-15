import { LineBuffer, parseLine } from './parser.js';
import { DEFAULT_BAUD_RATE } from './config.js';

// A data source backed by the Web Serial API. Only available in Chromium browsers.
// onLine(rawLine, values) fires for every received line; `values` is null for
// malformed/dropped lines (still reported so the debug view can show them).
export function createWebSerialSource({ onStatus, onLine }) {
  let port = null;
  let reader = null;
  let keepReading = false;

  const supported = 'serial' in navigator;

  async function connect(baudRate = DEFAULT_BAUD_RATE) {
    if (!supported) {
      onStatus('error', 'Web Serial API not available in this browser');
      return;
    }
    try {
      port = await navigator.serial.requestPort();
      await port.open({ baudRate });
      navigator.serial.addEventListener('disconnect', handleHardwareDisconnect);
      onStatus('connected', `Connected via Web Serial (${baudRate} baud)`);
      keepReading = true;
      readLoop();
    } catch (err) {
      onStatus('error', `Web Serial connect failed: ${err.message}`);
    }
  }

  function handleHardwareDisconnect(event) {
    if (event.target === port) {
      onStatus('disconnected', 'Device unplugged');
      keepReading = false;
    }
  }

  async function readLoop() {
    const decoder = new TextDecoder();
    const lineBuffer = new LineBuffer();
    reader = port.readable.getReader();
    try {
      while (keepReading) {
        const { value, done } = await reader.read();
        if (done) break;
        if (value) {
          const chunk = decoder.decode(value, { stream: true });
          for (const line of lineBuffer.push(chunk)) {
            onLine(line, parseLine(line));
          }
        }
      }
    } catch (err) {
      onStatus('error', `Read error: ${err.message}`);
    } finally {
      reader.releaseLock();
    }
  }

  async function disconnect() {
    keepReading = false;
    if (reader) {
      try {
        await reader.cancel();
      } catch (_) {
        // ignore, port may already be closed
      }
    }
    if (port) {
      try {
        await port.close();
      } catch (_) {
        // ignore
      }
    }
    onStatus('disconnected', 'Disconnected');
  }

  return { supported, connect, disconnect };
}
