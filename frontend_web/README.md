# frontend_web

Operator web UI for the [Auto Flight](../README.md) project's ground station. It's a Vue 3 + TypeScript single-page app that walks an operator through connecting to the [base_station](../base_station), planning a coverage area, reviewing the generated route, and monitoring the flight — talking to the base station's REST/WebSocket API described in [base_station/components/frontend](../base_station/components/frontend/README.md).

In production this app isn't hosted anywhere separate: it's built to static files and flashed into the base station's LittleFS partition, which the firmware then serves itself (see [Building & preparing for flashing](#building--preparing-for-flashing) below).

## What it does

- **Mission wizard**: a step-by-step flow (`src/stores/store.ts`'s `ConfigurationState`) that gates progress through `Connection → Area Selection → Route Approval → Starting → Flying → Finishing`. The current step is shown in the always-visible [StepView](src/components/StepView.vue) sidebar.
- **Connection overview** ([ConnectionOverview.vue](src/views/ConnectionOverview.vue)): live status of the base station and plane links, plus their sub-links (GPS, barometer, motor control, magnetometer), polled from `GET /api/status`. The wizard only allows moving on once everything reports connected.
- **Area planner** ([AreaPlanner.vue](src/views/AreaPlanner.vue) + [PolygonMap.vue](src/components/PolygonMap.vue)): draw a coverage polygon on a Leaflet map (via `leaflet-draw`) over the live base station/plane positions, then submit it with `POST /api/area`.
- **Route preview** ([RoutePreviewView.vue](src/views/RoutePreviewView.vue) + [RouteMap.vue](src/components/RouteMap.vue)): polls `GET /api/route` until the base station has computed a coverage route from the submitted polygon, then renders it alongside the flown flight history.
- **Live telemetry**: a WebSocket connection (`GET /api/ws`, wired up in `src/stores/store.ts` / `src/stores/websocketMessage.ts`) receives pushed `flight`, `connection` and `sensor` packets every 5 seconds and keeps the Pinia-less reactive store (`src/stores/store.ts`) up to date across the whole app — base/plane position, flown route, planned route, connection states and barometric pressure/altitude.
- **Connection status button** ([ConnectionStatusButton.vue](src/components/ConnectionStatusButton.vue)): persistent header control showing base/plane link state at a glance.

The WebSocket/REST packet shapes consumed here mirror the backend's `FrontendPackets.hpp` — a copy is kept at [references/FrontendPackets.hpp](references/FrontendPackets.hpp) for reference; the TypeScript equivalents live under [src/types](src/types) and [src/stores/websocketMessageParser.ts](src/stores/websocketMessageParser.ts).

## Tech stack

- [Vue 3](https://vuejs.org/) (`<script setup>`, Composition API) + [Vite](https://vite.dev/) + TypeScript
- [Vue Router](https://router.vuejs.org/) for the view/wizard navigation ([src/router/index.ts](src/router/index.ts))
- [Leaflet](https://leafletjs.com/) / [@vue-leaflet/vue-leaflet](https://github.com/vue-leaflet/vue-leaflet) + [leaflet-draw](https://github.com/Leaflet/Leaflet.draw) for the map and polygon drawing
- [@vueuse/core](https://vueuse.org/) for the WebSocket client and responsive-layout helpers
- [Playwright](https://playwright.dev/) for end-to-end tests, [ESLint](https://eslint.org/) + [oxlint](https://oxc.rs/docs/guide/usage/linter) for linting, [Prettier](https://prettier.io/) for formatting

## Prerequisites

- [Node.js](https://nodejs.org/) `^20.19.0` or `>=22.12.0`, and npm (see `engines` in [package.json](package.json))
- A base station reachable on the network — either the real [base_station](../base_station) firmware, or `WIFI_DEV_MODE` pointing it at your dev WiFi (see the [base_station README](../base_station/README.md#configuration))

## Project setup

```sh
npm install
```

### Compile and hot-reload for development

```sh
npm run dev
```

By default, Vite proxies `/api` (REST) and `/api/ws` (WebSocket) to a hardcoded base station IP in [vite.config.ts](vite.config.ts) (`server.proxy`). Change that IP to match your base station's address on the network before running `npm run dev` — otherwise the app loads but every API call/WebSocket connection will fail.

## Testing

### Type-checking and linting

```sh
npm run type-check   # vue-tsc --build
npm run lint         # oxlint --fix, then eslint --fix
```

### End-to-end tests (Playwright)

```sh
# Install browsers for the first run
npx playwright install

# When testing on CI, must build the project first
npm run build

# Runs the end-to-end tests
npm run test:e2e
# Runs the tests only on Chromium
npm run test:e2e -- --project=chromium
# Runs the tests of a specific file
npm run test:e2e -- tests/example.spec.ts
# Runs the tests in debug mode
npm run test:e2e -- --debug
```

`playwright.config.ts` starts `npm run preview` against the built `dist/` output, so the e2e suite exercises the same static files that get flashed to the base station, not the dev server.

## Building & preparing for flashing

The base station firmware serves this app's build output directly from flash — there is no separate web server to deploy to. To produce the files the firmware expects:

```sh
npm run build   # type-checks, then builds to dist/
```

This runs `vue-tsc --build` followed by `vite build`, outputting static files to `frontend_web/dist`. [`base_station/static`](../base_station/static) is a symlink to `../frontend_web/dist`, and the base station's [CMakeLists.txt](../base_station/CMakeLists.txt) packages that `static` directory into a LittleFS image for the `storage` partition (`littlefs_create_partition_image(storage static FLASH_IN_PROJECT)`), which `idf.py flash` writes to the device alongside the firmware image.

So, to get a UI change onto real hardware:

1. `npm run build` here — the resulting `dist/` is picked up automatically via the `static` symlink.
2. `cd ../base_station && idf.py build` — bundles `static/` (i.e. this app's `dist/`) into the `storage` LittleFS partition.
3. `idf.py -p <PORT> flash monitor` — flashes both the firmware and the partition image.

See the [base_station README](../base_station/README.md#building--running) for the full firmware build/flash walkthrough. For iterating on the UI itself, prefer `npm run dev` against a running base station (see above) — no rebuild/reflash needed for frontend-only changes.

## Project layout

| Path | What it is |
|---|---|
| [src/views](src/views) | Top-level wizard steps/pages routed via [src/router](src/router): connection overview, area planner, route preview, map. |
| [src/components](src/components) | Shared UI: step sidebar, connection status button, the two Leaflet map wrappers (`PolygonMap`, `RouteMap`). |
| [src/stores](src/stores) | Reactive app state (`store.ts`), WebSocket message handling and packet parsing/normalization. |
| [src/types](src/types) | TypeScript types for the connection/flight/sensor/coordinate packets, mirroring the backend's `FrontendPackets.hpp`. |
| [src/api](src/api) | Small REST fetch helpers (e.g. `status.ts`). |
| [src/composables](src/composables) | Reusable composition-API logic (e.g. `useConnectionItems`). |
| [references/FrontendPackets.hpp](references/FrontendPackets.hpp) | Copy of the backend packet definitions, kept for reference when updating the TS types. |
| [e2e](e2e) | Playwright end-to-end tests, run against the built `dist/` output. |
