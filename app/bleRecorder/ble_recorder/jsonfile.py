from __future__ import annotations

import json
import os
from typing import Any

_CLOSING = b"\n]\n"


class JSONArrayFile:
    """Appends entries to a JSON array file while keeping it a valid JSON document after every
    write: each append overwrites the trailing "\\n]" and writes it again after the new entry,
    so a crash or Ctrl-C never leaves a truncated file behind."""

    def __init__(self, path: str):
        self.path = path
        self.count = 0
        self._file = open(path, "w+b")
        self._file.write(b"[" + _CLOSING)
        self._file.flush()

    def append(self, entry: Any) -> None:
        text = json.dumps(entry, indent=2, ensure_ascii=False, allow_nan=False)
        # Indent the entry by one level so the file reads like a pretty-printed array.
        indented = "\n".join("  " + line for line in text.split("\n"))
        separator = "\n" if self.count == 0 else ",\n"
        self._file.seek(-len(_CLOSING), os.SEEK_END)
        self._file.write((separator + indented).encode("utf-8") + _CLOSING)
        self._file.flush()
        self.count += 1

    def close(self) -> None:
        if not self._file.closed:
            self._file.flush()
            os.fsync(self._file.fileno())
            self._file.close()
