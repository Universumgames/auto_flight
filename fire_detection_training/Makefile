# TensorFlow wheels lag behind new CPython releases by months; pin venv
# creation to a Python known to have a tensorflow wheel (3.9-3.12) instead
# of whatever `python3` happens to resolve to.
SYSTEM_PYTHON := $(shell command -v python3.11 || command -v python3.12 || command -v python3.10 || command -v /opt/homebrew/opt/python@3.11/bin/python3.11)
VENV := .venv
PYTHON := $(VENV)/bin/python

.PHONY: venv download prepare train convert export evaluate pipeline clean

venv:
	@if [ -z "$(SYSTEM_PYTHON)" ]; then \
		echo "No Python 3.10-3.12 found (TensorFlow has no wheels for newer Pythons yet)."; \
		echo "Install one, e.g.: brew install python@3.11"; \
		exit 1; \
	fi
	$(SYSTEM_PYTHON) -m venv $(VENV)
	$(PYTHON) -m pip install --upgrade pip
	$(PYTHON) -m pip install -r requirements.txt

download:
	$(PYTHON) download_dataset.py

prepare:
	$(PYTHON) prepare_dataset.py

train:
	$(PYTHON) train.py

convert:
	$(PYTHON) convert_to_tflite.py

export:
	$(PYTHON) export_c_array.py

evaluate:
	$(PYTHON) evaluate.py

# Full pipeline, assuming data/raw/<class>/*.jpg is already populated.
pipeline: prepare train convert evaluate export

clean:
	rm -rf models data/train data/val data/test
