import { parseLine } from './parser.js';

// A data source backed by the Python serial-to-WebSocket bridge
// (bridge/serial_to_ws_bridge.py). Works in any browser.
// onLine(rawLine, values) fires for every received line; `values` is null for
// malformed/dropped lines (still reported so the debug view can show them).
export function createWsBridgeSource({ onStatus, onLine }) {
  let socket = null;

  function connect(url) {
    try {
      socket = new WebSocket(url);
    } catch (err) {
      onStatus('error', `Invalid WebSocket URL: ${err.message}`);
      return;
    }

    onStatus('connecting', `Connecting to ${url}...`);

    socket.addEventListener('open', () => {
      onStatus('connected', `Connected via WS Bridge (${url})`);
    });

    socket.addEventListener('message', (event) => {
      // Each message is expected to be a single CSV line; also tolerate
      // multiple newline-joined lines in one message (and a trailing empty
      // segment from the sender's own newline terminator).
      for (const line of event.data.split('\n')) {
        if (line.trim() === '') continue;
        onLine(line, parseLine(line));
      }
    });

    socket.addEventListener('close', () => {
      onStatus('disconnected', 'WS Bridge connection closed');
    });

    socket.addEventListener('error', () => {
      onStatus('error', 'WS Bridge connection error');
    });
  }

  function disconnect() {
    if (socket) {
      socket.close();
      socket = null;
    }
    onStatus('disconnected', 'Disconnected');
  }

  return { supported: 'WebSocket' in window, connect, disconnect };
}
