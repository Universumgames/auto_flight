# Dependencies and the Python version (see .python-version) are managed by uv;
# `uv run` syncs .venv against uv.lock before each command.
RUN := uv run

.PHONY: venv download prepare train convert export evaluate pipeline clean

venv:
	uv sync

download:
	$(RUN) download_dataset.py

prepare:
	$(RUN) prepare_dataset.py

train:
	$(RUN) train.py

convert:
	$(RUN) -m model_helper.modelConverter

export:
	$(RUN) model_helper/cExport.py

evaluate:
	$(RUN) evaluate.py

# Full pipeline, assuming data/raw/<class>/*.jpg is already populated.
pipeline: prepare train convert evaluate export

clean:
	rm -rf models data/train data/val data/test
