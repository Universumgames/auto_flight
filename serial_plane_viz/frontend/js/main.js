import { createPlaneScene } from './plane.js';
import { createWebSerialSource } from './webserial.js';
import { createWsBridgeSource } from './wsbridge.js';
import { CHANNEL_ORDER, DEFAULT_WS_URL, DEFAULT_BAUD_RATE } from './config.js';

const canvas = document.getElementById('scene');
const plane = createPlaneScene(canvas);

const statusDot = document.getElementById('status-dot');
const statusText = document.getElementById('status-text');
const channelReadout = document.getElementById('channel-readout');
const channelOrderLabel = document.getElementById('channel-order');

const webSerialButton = document.getElementById('connect-webserial');
const wsBridgeButton = document.getElementById('connect-wsbridge');
const disconnectButton = document.getElementById('disconnect');
const wsUrlInput = document.getElementById('ws-url');
const baudInput = document.getElementById('baud-rate');

const debugToggle = document.getElementById('debug-toggle');
const debugPanel = document.getElementById('debug-panel');
const debugLog = document.getElementById('debug-log');

channelOrderLabel.textContent = CHANNEL_ORDER.join(', ');
wsUrlInput.value = DEFAULT_WS_URL;
baudInput.value = DEFAULT_BAUD_RATE;

let activeSource = null;

function setStatus(state, message) {
  statusDot.className = `status-dot status-${state}`;
  statusText.textContent = message;
  const connected = state === 'connected';
  const connecting = state === 'connecting';
  webSerialButton.disabled = connected || connecting || !webSerial.supported;
  wsBridgeButton.disabled = connected || connecting;
  disconnectButton.disabled = !connected && !connecting;
}

function timestamp() {
  const d = new Date();
  return d.toTimeString().slice(0, 8) + '.' + String(d.getMilliseconds()).padStart(3, '0');
}

function showDebugLine(rawLine, values) {
  if (values) {
    debugLog.className = 'debug-row';
    const parsed = CHANNEL_ORDER.map((name) => `${name}=${values[name].toFixed(2)}`).join(' ');
    debugLog.textContent = `${timestamp()}  raw: ${rawLine}   parsed: ${parsed}`;
  } else {
    debugLog.className = 'debug-row debug-row-dropped';
    debugLog.textContent = `${timestamp()}  raw: ${rawLine}   [dropped: malformed]`;
  }
}

function onLine(rawLine, values) {
  showDebugLine(rawLine, values);
  if (!values) return;
  plane.update(values);
  channelReadout.textContent = CHANNEL_ORDER.map(
    (name) => `${name}: ${values[name].toFixed(2)}`
  ).join('   ');
}

debugToggle.addEventListener('click', () => {
  const hidden = debugPanel.classList.toggle('hidden');
  debugToggle.textContent = hidden ? 'Show Debug' : 'Hide Debug';
});

const webSerial = createWebSerialSource({ onStatus: setStatus, onLine });
const wsBridge = createWsBridgeSource({ onStatus: setStatus, onLine });

if (!webSerial.supported) {
  webSerialButton.disabled = true;
  webSerialButton.title = 'Web Serial API is not available in this browser (use Chrome/Edge, or the WS Bridge option)';
}

webSerialButton.addEventListener('click', async () => {
  activeSource = webSerial;
  await webSerial.connect(Number(baudInput.value) || DEFAULT_BAUD_RATE);
});

wsBridgeButton.addEventListener('click', () => {
  activeSource = wsBridge;
  wsBridge.connect(wsUrlInput.value || DEFAULT_WS_URL);
});

disconnectButton.addEventListener('click', async () => {
  if (activeSource) {
    await activeSource.disconnect();
    activeSource = null;
  }
});

setStatus('disconnected', 'Disconnected');
