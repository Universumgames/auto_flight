import argparse
from pathlib import Path

import tensorflow

from config import MODEL_TFLITE_PATH


def get_model_ops(tfLiteModelFile: Path):
    tensorflow.lite.experimental.Analyzer.analyze(model_path=tfLiteModelFile)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-m", "--model", default=str(MODEL_TFLITE_PATH), help=".tflite model to get metadata out of")

    args = parser.parse_args()

    get_model_ops(args.model)

if __name__ == "__main__":
    main()