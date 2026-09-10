"""Small helpers for slicing/renaming pieces of KiCad's S-expression symbol
format without a full parser -- used to embed standard-library and
generated symbols into a schematic's (lib_symbols) block, and to read back
pin (number -> local x/y) geometry from any such block (ours or a standard
library's) so labels can be placed exactly on a pin's connection point.
"""
import re


def find_balanced(text: str, start_idx: int) -> str:
    """Return the balanced parenthesized expression starting at start_idx
    (which must point at an opening '(')."""
    depth = 0
    started = False
    for i in range(start_idx, len(text)):
        if text[i] == "(":
            depth += 1
            started = True
        elif text[i] == ")":
            depth -= 1
            if started and depth == 0:
                return text[start_idx:i + 1]
    raise ValueError("unbalanced expression")


def extract_symbol_block(text: str, bare_name: str) -> str:
    """Return the raw `(symbol "bare_name" ...)` block (tab-indented, as
    found in a standalone .kicad_sym library file)."""
    key = f'\t(symbol "{bare_name}"'
    idx = text.index(key)
    return find_balanced(text, idx + 1)  # +1: skip the leading tab, start at '('


def rename_symbol_block(block: str, bare_name: str, qualified_name: str) -> str:
    """Rename only the top-level symbol's own name to how KiCad expects an
    embedded lib_symbols entry to be named ("Libnickname:EntryName"). Nested
    unit sub-symbols keep their bare "EntryName_unitId_styleId" form --
    that's a UNIT_ID, not a LIBRARY_ID, and is never library-qualified."""
    return block.replace(f'"{bare_name}"', f'"{qualified_name}"', 1)


def extract_pins(symbol_block: str) -> dict:
    """Return {pin_number: (x, y, angle_deg)} for every `(pin ...)` token in
    a symbol block, found anywhere in it (works whether pins live in a
    "_0_1", "_1_1", or "_0_0" nested unit, we don't need to care which)."""
    pins = {}
    idx = 0
    while True:
        m = re.compile(r"\(pin\s+\S+\s+\S+").search(symbol_block, idx)
        if not m:
            break
        block = find_balanced(symbol_block, m.start())
        at_m = re.search(r"\(at\s+([-\d.]+)\s+([-\d.]+)\s+([-\d.]+)\)", block)
        num_m = re.search(r'\(number\s+"([^"]*)"', block)
        if at_m and num_m:
            pins[num_m.group(1)] = (float(at_m.group(1)), float(at_m.group(2)), float(at_m.group(3)))
        idx = m.start() + len(block)
    return pins
