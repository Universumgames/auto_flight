# Build and run ble-recorder.
#
#   make                      release build
#   make run                  build and record to recording-<timestamp>.json
#   make run OUT=flight.json NAME=base RAW=1 DURATION=60
#   make run TCP=7878 SOCKET=/tmp/ble-recorder.sock NOFILE=1
#   make debug / run-debug    same with a debug build
#   make install              copy the binary to $(PREFIX)/bin

SWIFT     ?= swift
PRODUCT   := ble-recorder
CONFIG    ?= release
PREFIX    ?= /usr/local

# Optional recorder arguments, see `ble-recorder --help`.
OUT  ?=
NAME ?=
RAW  ?=
DURATION ?=
TCP      ?=
SOCKET   ?=
NOFILE   ?=

BIN_DIR := $(shell $(SWIFT) build -c $(CONFIG) --show-bin-path 2>/dev/null)
BINARY  := $(BIN_DIR)/$(PRODUCT)
SOURCES := Package.swift $(wildcard Sources/BLERecorder/*.swift) Sources/BLERecorder/Info.plist

RUN_ARGS := $(if $(OUT),-o $(OUT)) $(if $(NAME),-n $(NAME)) $(if $(DURATION),-t $(DURATION)) $(if $(RAW),--raw) \
            $(if $(TCP),--tcp $(TCP)) $(if $(SOCKET),--socket $(SOCKET)) $(if $(NOFILE),--no-file) $(ARGS)

.PHONY: all build debug run run-debug install uninstall clean help

all: build

build: $(BINARY)

$(BINARY): $(SOURCES)
	$(SWIFT) build -c $(CONFIG) --product $(PRODUCT)

debug:
	$(MAKE) build CONFIG=debug

run: build
	$(BINARY) $(RUN_ARGS)

run-debug:
	$(MAKE) run CONFIG=debug

install: build
	install -d $(PREFIX)/bin
	install -m 755 $(BINARY) $(PREFIX)/bin/$(PRODUCT)

uninstall:
	rm -f $(PREFIX)/bin/$(PRODUCT)

clean:
	$(SWIFT) package clean
	rm -rf .build

help:
	@sed -n '2,9p' Makefile | sed 's/^# \{0,1\}//'
