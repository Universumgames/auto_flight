VENV := bridge/.venv
VENV_MARKER := $(VENV)/bin/activate

$(VENV_MARKER): bridge/requirements.txt
	python3 -m venv $(VENV)
	$(VENV)/bin/pip install -q -r bridge/requirements.txt

venv: $(VENV_MARKER)

start: venv
	$(VENV)/bin/python3 bridge/serial_to_ws_bridge.py

mock: venv
	$(VENV)/bin/python3 bridge/serial_to_ws_bridge.py --mock

.PHONY: venv start
