#!/usr/bin/env python3
"""Generate simplified STEP models for the I2C breakout boards, the GPS
module and the Arduino Nano into
hardware/circuits/kicad_libs/auto_flight.3dshapes/.

Needs CadQuery, which isn't a dependency of the other generators -- run it
from a throwaway venv:

    uv venv -p 3.12 .venv && uv pip install -p .venv cadquery
    .venv/bin/python gen_3d_models.py

Like the Heltec placeholder, these are recognizable approximations (board
outline, header row, mounting holes, main chips), NOT vendor CAD: outlines
come from common listings / the Fritzing breadboard SVGs, component
placement is approximate. Good for the 3D viewer and rough clearance
checks, not for precision fit.

Model frame (matches a KiCad PinHeader_1xNN footprint): pin 1 at the
origin, the header row running along footprint +Y (= model -Y, KiCad flips
Y), the board body extending to +X, board bottom at z=0. The PCB's
`(model ...)` entry lifts it by the header's 2.54mm plastic spacer and, if
the board should extend to -X instead, rotates it 180 deg (see README).
The level shifter is centered on its own 2x6 footprint instead. The Nano
matches auto_flight:Arduino_Nano (pin 1 at the origin, pads 1-15 down +Y)
and, unlike the others, includes its own soldered male headers below z=0.
"""
from pathlib import Path

import cadquery as cq

HERE = Path(__file__).parent
OUT_DIR = HERE.parent / "kicad_libs" / "auto_flight.3dshapes"

PITCH = 2.54
PCB_T = 1.6

PCB_BLUE = cq.Color(0.05, 0.25, 0.65)
PCB_PURPLE = cq.Color(0.4, 0.15, 0.55)
PCB_GREEN = cq.Color(0.1, 0.4, 0.2)
IC_BLACK = cq.Color(0.08, 0.08, 0.08)
METAL = cq.Color(0.8, 0.8, 0.82)
GOLD = cq.Color(0.85, 0.7, 0.3)
PASSIVE = cq.Color(0.55, 0.45, 0.35)
LED_RED = cq.Color(0.9, 0.1, 0.1)
LED_GREEN = cq.Color(0.1, 0.8, 0.2)
BUTTON = cq.Color(0.9, 0.9, 0.9)


class Board:
    """Builds in footprint coordinates (x right, y down); flipped to KiCad's
    model frame (y up) on export."""

    def __init__(self, name, x0, y0, w, h, color):
        self.name = name
        self.asm = cq.Assembly(name=name)
        self.pcb = cq.Workplane("XY").center(x0 + w / 2, y0 + h / 2).rect(w, h).extrude(PCB_T)
        self.color = color
        self.rings = []
        self.parts = []

    def hole(self, x, y, d, ring=None):
        self.pcb = self.pcb.cut(cq.Workplane("XY").center(x, y).circle(d / 2).extrude(PCB_T))
        if ring:
            r = (cq.Workplane("XY").workplane(offset=PCB_T).center(x, y)
                 .circle(ring / 2).circle(d / 2).extrude(0.05))
            self.rings.append(r)

    def header_holes(self, x, y, n, dx=0.0, dy=PITCH):
        for i in range(n):
            self.hole(x + i * dx, y + i * dy, 1.0, ring=1.7)

    def box(self, name, x, y, w, h, t, color, rot=0.0, z=PCB_T):
        """Component centered at (x, y), sitting on the board's top face
        (or at height z, e.g. z=-t for the bottom side), turned rot deg."""
        shape = (cq.Workplane("XY").workplane(offset=z).center(x, y)
                 .rect(w, h).extrude(t).edges("|Z").fillet(min(w, h) * 0.08)
                 .rotate((x, y, 0), (x, y, 1), rot))
        self.parts.append((name, shape, color))

    def cylinder(self, name, x, y, d, t, color, z=PCB_T):
        shape = cq.Workplane("XY").workplane(offset=z).center(x, y).circle(d / 2).extrude(t)
        self.parts.append((name, shape, color))

    def male_header(self, name, x, y, n, dx=0.0, dy=PITCH):
        """Male pin row soldered under the board: 2.54mm plastic spacer
        below the board, pins from 3mm under the spacer to just above the
        top face."""
        for i in range(n):
            px, py = x + i * dx, y + i * dy
            self.box(f"{name}_spacer{i}", px, py, PITCH, PITCH, PITCH, IC_BLACK, z=-PITCH)
            pin = (cq.Workplane("XY").workplane(offset=-PITCH - 3.0).center(px, py)
                   .rect(0.64, 0.64).extrude(PITCH + 3.0 + PCB_T + 1.0))
            self.parts.append((f"{name}_pin{i}", pin, GOLD))

    def chip_0603(self, name, x, y, rot=0):
        w, h = (1.6, 0.8) if rot == 0 else (0.8, 1.6)
        self.box(name, x, y, w, h, 0.45, PASSIVE)

    def save(self):
        flip = lambda s: s.mirror("XZ")  # footprint y-down -> model y-up
        self.asm.add(flip(self.pcb), name="pcb", color=self.color)
        for i, r in enumerate(self.rings):
            self.asm.add(flip(r), name=f"ring{i}", color=GOLD)
        for name, shape, color in self.parts:
            self.asm.add(flip(shape), name=name, color=color)
        out = OUT_DIR / f"{self.name}.step"
        self.asm.save(str(out), exportType="STEP")
        print(f"wrote {out}")


def gy521():
    """GY-521 MPU6050: ~21.2 x 16mm, 8-pin header along the long edge,
    2x 3mm mounting holes on the opposite edge."""
    n, L, W = 8, 21.2, 16.0
    y0 = -(L - (n - 1) * PITCH) / 2
    b = Board("GY-521_MPU6050", -1.5, y0, W, L, PCB_BLUE)
    b.header_holes(0, 0, n)
    b.hole(W - 4.0, y0 + 2.6, 3.0, ring=5.0)
    b.hole(W - 4.0, y0 + L - 2.6, 3.0, ring=5.0)
    b.box("MPU6050", 6.5, (n - 1) * PITCH / 2, 4.0, 4.0, 0.9, IC_BLACK)
    b.box("LDO", 4.0, 1.0, 2.9, 1.6, 1.1, IC_BLACK)
    b.box("LED", 4.0, 16.0, 1.6, 0.8, 0.6, LED_RED)
    for i, (x, y) in enumerate([(3.0, 5.0), (3.0, 7.0), (3.0, 11.0), (3.0, 13.0), (10.0, 4.0), (10.0, 14.0)]):
        b.chip_0603(f"R{i}", x, y, rot=1)
    b.save()


def gy271():
    """GY-271 QMC5883L/HMC5883L: ~14 x 13.5mm, 5-pin header (VCC GND SCL
    SDA DRDY), 2x 3mm mounting holes on the far side."""
    n, L, W = 5, 14.0, 13.5
    y0 = -(L - (n - 1) * PITCH) / 2
    b = Board("GY-271_Magnetometer", -1.5, y0, W, L, PCB_BLUE)
    b.header_holes(0, 0, n)
    b.hole(W - 4.0, y0 + 2.5, 3.0, ring=4.5)
    b.hole(W - 4.0, y0 + L - 2.5, 3.0, ring=4.5)
    b.box("QMC5883L", 5.0, (n - 1) * PITCH / 2, 3.0, 3.0, 0.9, IC_BLACK)
    b.box("LDO", 8.5, (n - 1) * PITCH / 2, 1.6, 2.9, 1.1, IC_BLACK)
    for i, y in enumerate([0.5, 2.5, 7.5]):
        b.chip_0603(f"C{i}", 2.8, y, rot=1)
    b.save()


def bmp280():
    """4-pin GY-BMP280 (VCC GND SCL SDA): ~11.5 x 10.5mm, one 2.5mm hole."""
    n, L, W = 4, 11.5, 10.5
    y0 = -(L - (n - 1) * PITCH) / 2
    b = Board("GY-BMP280_4pin", -1.3, y0, W, L, PCB_PURPLE)
    b.header_holes(0, 0, n)
    b.hole(W - 3.8, y0 + L - 2.2, 2.5, ring=4.0)
    b.box("BMP280", 5.5, 2.0, 2.0, 2.5, 0.95, METAL)
    b.box("LDO", 4.0, 6.5, 1.6, 2.9, 1.1, IC_BLACK)
    b.chip_0603("C0", 7.5, 4.0, rot=1)
    b.chip_0603("R0", 2.5, 2.5, rot=1)
    b.save()


def ads1115():
    """ADS1115 breakout (Adafruit layout, per the Fritzing breadboard SVG):
    28 x 16.6mm, 10-pin header 1.9mm from the long edge, 2x 2.5mm holes
    on the far side."""
    n, L, W = 10, 28.0, 16.6
    y0 = -(L - (n - 1) * PITCH) / 2
    b = Board("ADS1115_Breakout", -1.9, y0, W, L, PCB_BLUE)
    b.header_holes(0, 0, n)
    b.hole(W - 4.4, y0 + 2.5, 2.5, ring=4.5)
    b.hole(W - 4.4, y0 + L - 2.5, 2.5, ring=4.5)
    b.box("ADS1115", 7.0, (n - 1) * PITCH / 2, 3.0, 3.0, 1.1, IC_BLACK)
    for i, y in enumerate([5.0, 7.0, 15.0, 17.0]):
        b.chip_0603(f"R{i}", 3.5, y, rot=1)
    b.chip_0603("C0", 10.0, 11.43, rot=1)
    b.save()


def level_shifter():
    """4-channel BSS138 level shifter, centered on auto_flight:
    I2C_Level_Converter (2x6 pins, rows at y=+-5.75): 15 x 13.2mm board,
    one SOT-23 MOSFET + two pull-ups per channel."""
    b = Board("I2C_Level_Converter", -7.5, -6.6, 15.0, 13.2, PCB_BLUE)
    b.header_holes(-6.35, -5.75, 6, dx=PITCH, dy=0)
    b.header_holes(-6.35, 5.75, 6, dx=PITCH, dy=0)
    for i, x in enumerate([-5.4, -1.8, 1.8, 5.4]):
        b.box(f"Q{i}", x, 0, 2.9, 1.3, 1.0, IC_BLACK)
        b.chip_0603(f"RH{i}", x, -2.8, rot=0)
        b.chip_0603(f"RL{i}", x, 2.8, rot=0)
    b.save()


def gt_u8():
    """GT-U8 mini GPS: 13.66 x 16mm, 5-pin header (VCC GND TXD RXD PPS)
    along the 13.66mm edge, board extending 16mm away from it. GNSS module
    (10.1 x 9.7mm LCC) in the middle, IPEX antenna socket and PPS LED at the
    far edge, no mounting holes. Placement approximate."""
    n, L, W = 5, 13.66, 16.0
    y0 = -(L - (n - 1) * PITCH) / 2
    b = Board("GT-U8_GPS", -1.3, y0, W, L, PCB_BLUE)
    b.header_holes(0, 0, n)
    yc = (n - 1) * PITCH / 2
    b.box("GNSS", 6.7, yc, 9.7, 10.1, 2.4, METAL)
    b.box("IPEX", 13.35, yc - 3.6, 2.6, 2.6, 1.25, GOLD)
    b.box("LDO", 13.3, yc + 0.4, 1.6, 2.9, 1.1, IC_BLACK)
    b.box("LED_PPS", 13.3, yc + 3.8, 0.8, 1.6, 0.6, LED_GREEN)
    b.save()


def arduino_nano():
    """Arduino Nano (v3, 43.18 x 17.78mm), matching auto_flight:Arduino_Nano:
    pin 1 at the origin, rows at x=0 and x=15.24, pins 15/16 at the USB end
    (+y in footprint coords). Unlike the breakouts this one includes its
    soldered male headers (spacer under the board), so place it with
    `offset z 2.54` for a direct solder or socket height + 2.54 in female
    headers. Top side: mini-USB, ATmega328P (TQFP-32 at 45 deg), 16MHz
    crystal, reset button, LEDs, unpopulated ICSP holes; bottom side: USB
    serial chip and the 5V regulator."""
    b = Board("Arduino_Nano", -1.27, -3.81, 17.78, 43.18, PCB_BLUE)
    for col in (0.0, 15.24):
        b.header_holes(col, 0, 15)
        b.male_header(f"J{int(col)}", col, 0, 15)
    xc = 7.62
    for r in range(2):  # ICSP 2x3 at the far end from USB, left unpopulated
        for c in range(3):
            b.hole(xc - PITCH + c * PITCH, -1.8 + r * PITCH, 1.0, ring=1.7)
    b.box("MiniUSB", xc, 42.04 - 9.2 / 2, 7.7, 9.2, 3.9, METAL)
    b.box("ATmega328P", xc, 11.5, 7.0, 7.0, 1.2, IC_BLACK, rot=45)
    b.box("Crystal", xc + 3.5, 24.0, 3.2, 2.5, 0.8, METAL)
    b.box("Reset", xc, 20.0, 6.0, 3.5, 1.6, METAL)
    b.cylinder("Reset_cap", xc, 20.0, 2.2, 2.4, BUTTON)
    b.box("LDO_3V3", xc - 3.5, 28.5, 2.9, 1.6, 1.1, IC_BLACK)
    for i, (y, color) in enumerate([(26.5, LED_GREEN), (28.0, LED_RED), (29.5, LED_RED), (31.0, LED_RED)]):
        b.box(f"LED{i}", xc + 3.5, y, 1.6, 0.8, 0.6, color)
    for i, (x, y) in enumerate([(3.5, 4.5), (11.7, 4.5), (3.5, 17.0), (11.7, 17.0), (3.5, 24.0)]):
        b.chip_0603(f"R{i}", x, y, rot=1)
    b.box("CH340", xc, 18.0, 4.0, 10.0, 1.5, IC_BLACK, z=-1.5)
    b.box("AMS1117", xc, 6.0, 6.5, 7.0, 1.6, IC_BLACK, z=-1.6)
    b.box("Diode", xc, 30.0, 3.5, 1.6, 1.0, IC_BLACK, z=-1.0)
    b.save()


if __name__ == "__main__":
    OUT_DIR.mkdir(exist_ok=True)
    gy521()
    gy271()
    bmp280()
    ads1115()
    level_shifter()
    gt_u8()
    arduino_nano()
