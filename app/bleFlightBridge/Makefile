# Build and run ble-flight-bridge (Python + bleak; macOS, Linux and Windows).
#
#   make                      standalone executable in dist/ (sets up .venv on first run)
#   make run                  run from source, record to recording-<timestamp>.json
#   make run OUT=flight.json NAME=base RAW=1 DURATION=60
#   make run TCP=7878 SOCKET=/tmp/ble-flight-bridge.sock NOFILE=1
#   make run-dist             same, but with the standalone executable
#   make test                 unit tests
#   make install              copy the executable to $(PREFIX)/bin
#   make clean / distclean    remove build output / also the virtualenv

PYTHON  ?= python3
PRODUCT := ble-flight-bridge
PREFIX  ?= /usr/local
VENV    := .venv

# Optional arguments, see `ble-flight-bridge --help`.
OUT      ?=
NAME     ?=
RAW      ?=
DURATION ?=
TCP      ?=
SOCKET   ?=
NOFILE   ?=

ifeq ($(OS),Windows_NT)
    VENV_BIN := $(VENV)/Scripts
    EXE      := .exe
else
    VENV_BIN := $(VENV)/bin
    EXE      :=
endif
VENV_PYTHON := $(VENV_BIN)/python$(EXE)
VENV_STAMP  := $(VENV)/.installed
DIST        := dist/$(PRODUCT)$(EXE)
SOURCES     := main.py $(wildcard ble_flight_bridge/*.py)

RUN_ARGS := $(if $(OUT),-o $(OUT)) $(if $(NAME),-n $(NAME)) $(if $(DURATION),-t $(DURATION)) $(if $(RAW),--raw) \
            $(if $(TCP),--tcp $(TCP)) $(if $(SOCKET),--socket $(SOCKET)) $(if $(NOFILE),--no-file) $(ARGS)

.PHONY: all dist venv run run-dist test install uninstall clean distclean help

all: dist

venv: $(VENV_STAMP)

# Recreated whenever a requirements file changes.
$(VENV_STAMP): requirements.txt requirements-build.txt
	$(PYTHON) -m venv $(VENV)
	$(VENV_PYTHON) -m pip install --quiet --upgrade pip
	$(VENV_PYTHON) -m pip install --quiet -r requirements-build.txt
	@echo > $@

dist: $(DIST)

$(DIST): $(VENV_STAMP) $(SOURCES)
	$(VENV_PYTHON) -m PyInstaller --onefile --clean --noconfirm --log-level WARN \
		--name $(PRODUCT) --paths . \
		--distpath dist --workpath build/pyinstaller --specpath build \
		main.py
	@echo "Built $(DIST)"

run: $(VENV_STAMP)
	$(VENV_PYTHON) -m ble_flight_bridge $(RUN_ARGS)

run-dist: $(DIST)
	$(DIST) $(RUN_ARGS)

test: $(VENV_STAMP)
	$(VENV_PYTHON) -m unittest discover -s tests -t . -v

install: $(DIST)
	install -d $(PREFIX)/bin
	install -m 755 $(DIST) $(PREFIX)/bin/$(PRODUCT)$(EXE)

uninstall:
	rm -f $(PREFIX)/bin/$(PRODUCT)$(EXE)

clean:
	rm -rf build dist

distclean: clean
	rm -rf $(VENV)

help:
	@sed -n '2,10p' Makefile | sed 's/^# \{0,1\}//'
