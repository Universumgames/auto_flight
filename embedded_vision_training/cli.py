import argparse
from enum import Enum

from constants import DATASETS_DIR, MODELS_DIR, EVALUATIONS_DIR
from dataset_helper import download, load, commons
from model_helper import trainModel, evaluate, commons
from model_helper.modelPipeline import test_model_iteration


class Mode(Enum):
    DOWNLOAD = "download"
    TRAIN = "train"
    EVALUATE = "eval"
    FULL = "full"

    def __str__(self) -> str:
        return self.value


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--mode", type=Mode, choices=list(Mode), help="Set the operation mode")
    parser.add_argument("--slug", type=str, help="The data slug from kaggle to use")
    parser.add_argument("--image-size", type=int)
    parser.add_argument("--channels", type=int)
    parser.add_argument("--epochs", type=int)
    parser.add_argument("--batch-size", type=int)
    parser.add_argument("--learning-rate", type=float)

    args = parser.parse_args()

    match args.mode:
        case Mode.DOWNLOAD:
            download.download_ds_slug(args.slug, None, DATASETS_DIR / commons.slug_to_id(args.slug))
        case Mode.TRAIN:
            train_ds, val_ds, class_names = load.load_datasets(
                DATASETS_DIR / commons.slug_to_id(args.slug), commons.AUGMENTATION, args.image_size,
                args.batch_size)
            trainModel.train_new_model(commons.slug_to_id(args.slug), MODELS_DIR, args.channels,
                                       args.image_size, train_ds, val_ds, class_names, args.learning_rate, args.epochs)
        case Mode.EVALUATE:
            modelNames = trainModel.model_filenames(MODELS_DIR, commons.slug_to_id(args.slug))
            evaluation = evaluate.evaluate_tflite_model(modelNames["tflite"],
                                                        DATASETS_DIR / commons.slug_to_id(
                                                            args.slug) / "train",
                                                        args.image_size)
            evaluate.save_results(EVALUATIONS_DIR, evaluation)
        case Mode.FULL:
            test_model_iteration(args.slug, args.image_size, args.channels, args.batch_size, args.learning_rate,
                                 args.epochs, commons.AUGMENTATION, None)

    pass


if __name__ == "__main__":
    main()
