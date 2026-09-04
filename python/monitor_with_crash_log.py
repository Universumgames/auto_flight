#!/usr/bin/env python3
"""Run `idf.py monitor` normally, but watch the output for crash markers
(panics, aborts, backtraces, asserts, ...) and additionally dump each crash
plus N lines of context before/after it into its own timestamped file.

Usage:
    python3 scripts/monitor_with_crash_log.py [-- <extra idf.py monitor args>]
    python3 scripts/monitor_with_crash_log.py -p /dev/cu.usbserial-0001
    python3 scripts/monitor_with_crash_log.py --project flight_controller -p /dev/cu.usbserial-0001

If --project is omitted, you're prompted to pick flight_controller or base_station.

Requires a POSIX tty (macOS/Linux). Run it from an environment where
`idf.py` is on PATH (i.e. after sourcing export.sh / activate_idf_*.sh).
"""

import argparse
import collections
import datetime
import os
import pty
import re
import shutil
import sys

CRASH_PATTERNS = [
    re.compile(rb"Guru Meditation Error"),
    re.compile(rb"abort\(\) was called"),
    re.compile(rb"Backtrace:"),
    re.compile(rb"assert failed:"),
    re.compile(rb"\*\*\*ERROR\*\*\*"),
    re.compile(rb"CORRUPT HEAP"),
    re.compile(rb"stack smashing detected"),
    re.compile(rb"rst:.*\(RTCWDT|rst:.*\(TG.WDT"),
]

ANSI_RE = re.compile(rb"\x1b\[[0-9;]*[a-zA-Z]")


def strip_ansi(data: bytes) -> bytes:
    return ANSI_RE.sub(b"", data)


class CrashCapture:
    """Buffers recent lines and, once a crash marker is seen, keeps
    capturing until `after` more lines have arrived, then flushes
    before + trigger + after into a single timestamped log file."""

    def __init__(self, out_dir: str, before: int, after: int, project: str):
        self.out_dir = out_dir
        self.project = project
        self.before_lines: "collections.deque[bytes]" = collections.deque(maxlen=before)
        self.after_target = after
        self.active = False
        self.capture_lines: list[bytes] = []
        self.after_count = 0
        self._partial = b""

    def feed(self, data: bytes) -> None:
        self._partial += data
        parts = self._partial.split(b"\n")
        self._partial = parts.pop()
        for line in parts:
            self._handle_line(line)

    def _handle_line(self, raw_line: bytes) -> None:
        clean = strip_ansi(raw_line).rstrip(b"\r")
        ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
        stamped = f"{ts} ".encode() + clean

        if not self.active:
            if any(p.search(clean) for p in CRASH_PATTERNS):
                self.active = True
                self.capture_lines = list(self.before_lines) + [stamped]
                self.after_count = 0
            else:
                self.before_lines.append(stamped)
        else:
            self.capture_lines.append(stamped)
            self.after_count += 1
            if self.after_count >= self.after_target:
                self._flush()

    def _flush(self) -> None:
        os.makedirs(self.out_dir, exist_ok=True)
        name = datetime.datetime.now().strftime(f"crash_{self.project}_%Y%m%d_%H%M%S.log")
        path = os.path.join(self.out_dir, name)
        with open(path, "wb") as f:
            f.write(b"\n".join(self.capture_lines) + b"\n")
        sys.stderr.write(f"\n[crash-logger] wrote {path} ({len(self.capture_lines)} lines)\n")
        sys.stderr.flush()
        self.active = False
        self.capture_lines = []
        self.after_count = 0

    def close(self) -> None:
        # Flush whatever was captured even if we never reached `after` lines
        # (e.g. the board disconnected right after the crash).
        if self.active and self.capture_lines:
            self._flush()


REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROJECTS = ["flight_controller", "base_station"]


def prompt_for_project() -> str:
    print("Which idf.py project do you want to monitor?")
    for i, name in enumerate(PROJECTS, start=1):
        print(f"  {i}) {name}")
    while True:
        choice = input(f"Select [1-{len(PROJECTS)}]: ").strip()
        if choice.isdigit() and 1 <= int(choice) <= len(PROJECTS):
            return PROJECTS[int(choice) - 1]
        print("Invalid choice, try again.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--before", type=int, default=100, help="Lines of context to keep before a crash marker")
    parser.add_argument("--after", type=int, default=100, help="Lines of context to capture after a crash marker")
    parser.add_argument(
        "--out-dir",
        default=os.path.join(REPO_ROOT, "logs", "crashes"),
        help="Directory to write crash_<timestamp>.log files into",
    )
    parser.add_argument(
        "--project",
        choices=PROJECTS,
        help="Which idf.py project directory to run in (flight_controller or base_station). "
        "Prompted interactively if omitted.",
    )
    parser.add_argument("monitor_args", nargs=argparse.REMAINDER, help="Extra args passed through to `idf.py monitor`")
    args = parser.parse_args()

    project = args.project
    if project is None:
        if not sys.stdin.isatty():
            sys.exit("--project is required (flight_controller or base_station) when not running interactively")
        project = prompt_for_project()
    project_dir = os.path.join(REPO_ROOT, project)
    if not os.path.isfile(os.path.join(project_dir, "CMakeLists.txt")):
        sys.exit(f"No CMakeLists.txt found in {project_dir}")

    idf_py = shutil.which("idf.py")
    if idf_py is None:
        # Some activation scripts (e.g. eim's activate_idf_*.sh) only define
        # idf.py as a shell alias rather than putting it on PATH, so fall
        # back to resolving it via $IDF_PATH, which they do export.
        idf_path = os.environ.get("IDF_PATH")
        if idf_path:
            candidate = os.path.join(idf_path, "tools", "idf.py")
            if os.access(candidate, os.X_OK):
                idf_py = candidate
    if idf_py is None:
        sys.exit("idf.py not found on PATH or via $IDF_PATH - source export.sh / activate_idf_*.sh first")

    extra = args.monitor_args
    if extra and extra[0] == "--":
        extra = extra[1:]

    capture = CrashCapture(args.out_dir, args.before, args.after, project)

    def master_read(fd: int) -> bytes:
        data = os.read(fd, 4096)
        if data:
            capture.feed(data)
        return data

    try:
        pty.spawn([idf_py, "-C", project_dir, "monitor", *extra], master_read=master_read)
    finally:
        capture.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
