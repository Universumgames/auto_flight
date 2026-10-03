# Fritzing → KiCad conversion tooling

Generates the KiCad projects in `../base_station_pcb/` and `../plane_pcb/`
from the original Fritzing sketches in `../fritzing_source/`. There is no
reliable off-the-shelf Fritzing→KiCad converter, so this is a small custom
pipeline: parse Fritzing's XML into a clean part list + netlist, then
generate KiCad schematic/PCB/symbol/footprint files from that.

## Pipeline

```
fz_extract.py      .fzz -> <stem>.parts.json + <stem>.nets.json
gen_symbols.py      -> ../kicad_libs/auto_flight.kicad_sym
gen_footprints.py   -> ../kicad_libs/auto_flight.pretty/
gen_schematic.py    -> <project>.kicad_sch
gen_pcb.py          -> <project>.kicad_pcb
```

Re-run in that order after editing `pin_overrides.json` or any generator.
`fz_extract.py` must run for both boards before `gen_symbols.py` (it reads
both `*.parts.json` files to build the custom symbol library once).

```
python3 fz_extract.py ../fritzing_source/schematics_base.fzz \
  --parts-dir ../fritzing_source --parts-dir fritzing_core_parts_cache \
  --pin-overrides pin_overrides.json --out-dir ../base_station_pcb --stem base_station

python3 fz_extract.py ../fritzing_source/schematics_plane.fzz \
  --parts-dir ../fritzing_source --parts-dir fritzing_core_parts_cache \
  --pin-overrides pin_overrides.json --out-dir ../plane_pcb --stem plane

python3 gen_symbols.py
python3 gen_footprints.py
python3 gen_schematic.py ../base_station_pcb base_station base_station_pcb
python3 gen_schematic.py ../plane_pcb plane plane_pcb
python3 gen_pcb.py ../base_station_pcb base_station base_station_pcb
python3 gen_pcb.py ../plane_pcb plane plane_pcb
```

Validate with `kicad-cli`:
```
kicad-cli sch erc <project>.kicad_sch --output erc_report.txt --severity-all
kicad-cli pcb drc <project>.kicad_pcb --output drc_report.txt --severity-all
```

## What each script does

- **`fz_extract.py`**: unzips the `.fzz`, resolves each part's pin names from
  its embedded/standalone `.fzp`, and builds the netlist via union-find over
  `<connect>` edges (only `layer="schematicTrace"`/`"schematic"` count — a
  lot of the file's other connect entries are cross-view bookkeeping, not
  real connectivity; see the union-find comments in the script for the two
  bugs that took real effort to find: wire instances' own two ends aren't
  explicitly linked anywhere in the XML — has to be inferred — and Fritzing
  emits fake "schematic"-layer connects for breadboard graphics that were
  never actually placed in the schematic view). Prints a part/net summary —
  cross-check it against `../fritzing_source/schematics_*_schem.svg` and the
  main README before trusting the output.
- **`pin_overrides.json`**: manual pin-name assignments for the two parts
  with no `.fzp` anywhere (not embedded, not in a standalone `.fzpz`, not in
  Fritzing's own core-parts library) — the I2C level converter and the
  MPU6050 breakout. Names were inferred from *netlist context* (which real
  device pin each connector shares a net with), not a datasheet, though they
  match well-known standard breakout board pinouts.
- **`fritzing_core_parts_cache/`**: `.fzp` files fetched from
  [fritzing/fritzing-parts](https://github.com/fritzing/fritzing-parts) for
  parts that are Fritzing *core* parts (bundled with the app, not saved into
  the `.fzz`) rather than custom/contributed parts: the resistor, Arduino
  Nano, the SparkFun JST connector and NPN transistor, the Dagu servo
  header, and the breadboard (its `<buses>` table turned out unnecessary —
  see the note in `fz_extract.py` about breadboard graphics not being real
  schematic content).
- **`parts_catalog.py`**: per-`moduleIdRef` mapping to a KiCad symbol +
  footprint, and the pin-name → pin-number resolution needed to wire a
  placed symbol's pins to the extracted netlist. See the table below for the
  reasoning behind each part's assignment.
- **`gen_symbols.py`**: for parts with no reasonable existing KiCad symbol,
  generates a simple box-with-pins symbol directly from the resolved pin
  list — physical pin order comes from a `Pin N`/`pin N` name pattern when
  present (GT-U8, Heltec), else from the Fritzing connectorId's numeric
  suffix.
- **`gen_footprints.py`**: the footprints with no standard-library
  equivalent — the Heltec module's 2×18 THT header, and the I2C level
  converter's 2×6 THT header (2.54mm pitch, 11.5mm row spacing, given
  directly by the user, not estimated). The Heltec's row spacing is an
  **estimated** dimension — see below. It also attaches a 3D model at
  `../kicad_libs/auto_flight.3dshapes/Heltec_WiFi_LoRa_32_V3.2.step` — a
  user-supplied STEP export from GrabCAD ("Heltec V3 (ESP32 LoRa) —
  Simplified 3D Model for Mechanical Integration"), rotated 90° about Z to
  align its long axis with this footprint's pin-row axis (confirmed by
  `kicad-cli pcb render`, not by eye in the GUI — see "3D model sourcing"
  below for the reasoning and for the other candidate file that was
  considered and rejected). It has no embedded color/material data, so it
  renders as flat white/gray in KiCad's 3D viewer — that's the source file,
  not a bug. A hand-authored placeholder box
  (`Heltec_WiFi_LoRa_32_V3.2.wrl`) is still in the repo but no longer
  referenced; it's what turned up the one VRML gotcha worth recording here:
  KiCad's VRML importer treats raw coordinate values as millimeters, not the
  VRML-spec-default meters — a model authored in meters with `scale (1 1
  1)` silently renders 1000x too small to see, with no error from
  `kicad-cli` either at generation or render time.
- **`gen_schematic.py`**: places symbols on a plain grid (not Fritzing's
  coordinates — unrelated pin layouts, wouldn't produce sensible spacing)
  and wires same-net pins with coincident local labels (no routed wire
  geometry) plus one `power:PWR_FLAG` per net so ERC doesn't flag power-type
  pins as undriven. `NET_RENAME` cleans up the handful of net names that
  `fz_extract.py`'s majority-vote naming still couldn't resolve cleanly
  (mainly because the main ground net includes a transistor pin literally
  described "C", diluting the "GND" vote).
- **`gen_pcb.py`**: Edge.Cuts from the Fritzing board's declared width/
  height (or larger — see below), footprints on a plain grid, pads wired to
  the same nets. No copper routing.
- **`gen_3d_models.py`** (optional, needs CadQuery — run from a throwaway
  venv, see its docstring): simplified STEP models for the I2C breakouts
  (`GY-521_MPU6050`, `GY-271_Magnetometer`, `GY-BMP280_4pin`,
  `ADS1115_Breakout`, `I2C_Level_Converter`), the `GT-U8_GPS` module and
  the `Arduino_Nano` in
  `../kicad_libs/auto_flight.3dshapes/`. Board outline, header row,
  mounting holes and main chips only — approximations, not vendor CAD. Each
  model has pin 1 at its origin and the board extending to +X; they're
  attached per footprint instance in the `.kicad_pcb` files (not in
  `gen_pcb.py`, since the layouts are hand-edited now) with `offset z 2.54`
  so the module sits on its pin header's plastic spacer. Where the board has
  to extend to -X (the magnetometer on the plane PCB) the entry uses
  `rotate z 180` plus `offset y -(N-1)*2.54` to bring pin 1 back onto pad 1.
  The level shifter's model is centered on its own footprint, which also
  gets two `PinHeader_1x06` models (`rotate z -90`) for its two rows.
  `Arduino_Nano.step` fits the `auto_flight:Arduino_Nano` footprint as-is
  and brings its own male headers (spacer and pins below the board), so
  it needs `offset z 2.54` soldered directly, or socket height + 2.54 in
  female headers.

## Known limitations — check before treating this as finished

- **Layout is placeholder, not final.** Both the schematic and PCB use a
  plain auto-generated grid, not Fritzing's (or any) real layout. Expect to
  rearrange everything in KiCad before treating either as a real deliverable.
- **The plane's Edge.Cuts is bigger than the Fritzing source's declared
  71×93mm.** 22 real parts using generic THT header footprints don't fit
  that outline without overlapping — and the actual design already spans
  multiple physical boards (main board + at least one daughterboard, per the
  3D enclosure design in `3d_models/`), so a single small rectangle was
  never going to be accurate anyway. `gen_pcb.py` grows the outline to fit
  the generated grid instead of forcing artificial overlaps; redraw
  Edge.Cuts (and probably split into multiple boards) once parts are placed
  where they'll actually live.
- **`#DIMENSIONS-UNVERIFIED` — footprints needing a caliper/datasheet check
  before ordering boards:**
  - Heltec module footprint: pin pitch (2.54mm) and pins-per-row (18) are
    confirmed from Heltec's published specs; **row-to-row spacing (20.32mm)
    is an estimate**, not confirmed from a dimensioned drawing.
  - All the generic `Connector_PinHeader_2.54mm:PinHeader_1x0N` footprints
    used for breakout boards (GT-U8 GPS, BMP280, MPU6050, ADS1115, battery
    fuel gauge) assume a single 0.1"-pitch THT row, which is true for most
    such hobbyist breakouts but wasn't verified per board. Pin *order* along
    the header is arbitrary (not matched to any specific real board's
    silkscreen) — if you swap in a real board's actual footprint later,
    check its datasheet/silkscreen against the pin names on the matching
    KiCad symbol, not against pin position.
  - The I2C level converter uses a **custom** footprint
    (`auto_flight:I2C_Level_Converter`, generated by `gen_footprints.py`),
    not a generic header: 2 rows of 6 pins, 2.54mm pitch, 11.5mm row
    spacing, on a 15×13.2mm silkscreen board outline — all given directly by
    the user, none estimated. Pin 1 (HV) is the top-left square pad; the row
    layout mirrors the symbol's HV-side/LV-side split (pins 1–6 = HV row,
    7–12 = LV row directly across).
  - JST footprint pitch (`JST_EH_B4B-EH-A_1x04_P2.50mm_Vertical`, 2.5mm) is
    an assumption — confirm against the actual SparkFun M04 JST-PTH part
    before ordering.
  - `Package_TO_SOT_THT:TO-92_Inline` for the 2N2222 (U1): pad order (1=B,
    2=E, 3=C) is electrically correct per the netlist, but TO-92 pin-to-leg
    orientation is a classic real-world mixup — verify against the 2N2222
    datasheet before soldering.
- **The Heltec's 3D model is "simplified/non-exact" per its own source, not
  a precision scan.** GrabCAD/SnapEDA listings for this board sit behind
  account-gated downloads, so no automated sourcing was possible (see the
  "3D model sourcing" section below) — the user downloaded
  `Heltec_WiFi_LoRa_32_V3.2.step` themselves (GrabCAD: "Heltec V3 (ESP32
  LoRa) — Simplified 3D Model for Mechanical Integration") and it's good
  enough for a recognizable shape and rough height in KiCad's 3D viewer, but
  wasn't built for precision mechanical fit checks against the enclosures in
  `3d_models/` — treat it the same as the other estimated dimensions here.
  It has no color/material data, so it renders flat white/gray.

### 3D model sourcing

A second STEP file the user found (`HTIT-WB32LAF_V3.7(3).step`, ~7.5MB, kept
in `../kicad_libs/auto_flight.3dshapes/` but **not referenced by any
footprint**) turned out not to be usable: its `PRODUCT` entries are named
like an EasyEDA/JLCPCB ECAD→MCAD export (`3X4X2.0-Heltec`, individual `C1`.
`C17` capacitor placeholders, a `Board` product, 83 separate solids) rather
than a single mechanical-integration shape, and its point cloud spans about
137×34mm — roughly 2-3x a single Heltec board's real footprint, consistent
with a full SMT manufacturing panel (multiple board copies plus panel
rails) rather than one board. It's plausibly the *most* electrically
accurate of the two (real component placement, if the panel-vs-single-board
read is right), but turning it into a usable single-board footprint model
would mean isolating one board's worth of solids from the panel — not
attempted here. If you want to pursue it, `grep "PRODUCT('" ` on the file is
the starting point for identifying which solids belong to one board.
- **Resistor values (R1–R6) are unset** — the Fritzing source didn't carry
  ohm values into any pin/property data we extracted; fill them in from your
  design notes (`assets/i2c_pullup_resistor_paper.pdf` documents the I2C
  pullup calculation) before using the schematic as a BOM source.
- **Servo/SBUS connectors are reinterpreted, not modeled as their Fritzing
  part shape.** `J1`–`J4` and the SBUS receiver connector are simple 3-pin
  0.1" headers (`Connector_Generic:Conn_01x03`) in KiCad, since that's what
  the Fritzing file actually models electrically (see the audit notes in
  `fz_extract.py`'s docstring) — not a full SBUS-receiver-shaped part.
- **The magnetometer (HMC5883L) and TOF200C sensor aren't in either
  KiCad project.** They aren't in the Fritzing source either (confirmed:
  zero references in both `.fz` files) — this conversion reproduces the
  Fritzing source faithfully, it doesn't backfill circuitry that was never
  drawn there.
- **ERC/DRC "violations" that are expected, not bugs:** every genuinely
  unused pin (most of the Heltec's 36 GPIOs, Arduino Nano's unused VIN/3V3,
  etc.) shows as `pin_not_connected`/`power_pin_not_driven` in ERC and as
  unrouted ratsnest in DRC — that's accurate, not a generation defect. One
  cosmetic `silk_overlap` per board (reference text touching the footprint's
  own silkscreen outline) is likewise harmless.
