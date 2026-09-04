VENV := .venv
PYTHON := $(VENV)/bin/python3
PIP := $(VENV)/bin/pip
STAMP := $(VENV)/.installed
ESP_IDF_ACTIVATE := $(HOME)/.espressif/tools/activate_idf_v6.0.1.sh

.PHONY: help simple map monitor clean

help:
	@echo "Targets:"
	@echo "  make simple   - run path_visualizer_simple.py"
	@echo "  make map      - run path_visualizer_map.py"
	@echo "  make monitor  - run monitor_with_crash_log.py (sources ESP-IDF if needed)"
	@echo "                  pass extra args via ARGS, e.g.:"
	@echo "                  make monitor ARGS=\"--project flight_controller -p /dev/cu.usbserial-0001\""
	@echo "  make clean    - remove the venv"

$(VENV)/bin/python3:
	python3 -m venv $(VENV)

$(STAMP): $(VENV)/bin/python3 requirements.txt
	$(PIP) install -r requirements.txt
	touch $(STAMP)

simple: $(STAMP)
	$(PYTHON) path_visualizer_simple.py

map: $(STAMP)
	$(PYTHON) path_visualizer_map.py

monitor: $(STAMP)
	@if ! command -v idf.py >/dev/null 2>&1 && [ -z "$$IDF_PATH" ]; then \
		echo "ESP-IDF not found on PATH, sourcing $(ESP_IDF_ACTIVATE)..."; \
		. "$(ESP_IDF_ACTIVATE)"; \
	fi; \
	$(PYTHON) monitor_with_crash_log.py $(ARGS)

clean:
	rm -rf $(VENV)
