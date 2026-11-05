import argparse
from enum import Enum

import dataset_helper
import model_helper
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
    parser.add_argument("--learning-rate", type=float, default=1e-3)

    args = parser.parse_args()

    model_id = dataset_helper.commons.slug_to_id(args.slug)
    ds_root_dir = DATASETS_DIR / model_id

    match args.mode:
        case Mode.DOWNLOAD:
            download.download_ds_slug(args.slug, None, None)
        case Mode.TRAIN:
            train_ds, val_ds, class_names = load.load_training_datasets(
                ds_root_dir, model_helper.commons.AUGMENTATION, args.image_size,
                args.batch_size)
            trainModel.train_new_model(model_id, args.channels, args.image_size, train_ds, val_ds, class_names,
                                       args.learning_rate, args.epochs, None)
        case Mode.EVALUATE:
            model_names = model_helper.commons.model_filenames(model_id, None)
            evaluation = evaluate.evaluate_tflite_model(model_names["tflite"],
                                                        dataset_helper.commons.splitted_dataset_dirs(args.slug)["train"],
                                                        args.image_size)
            evaluate.save_results(EVALUATIONS_DIR, evaluation)
        case Mode.FULL:
            test_model_iteration(args.slug, args.image_size, args.channels, args.batch_size, args.learning_rate,
                                 args.epochs, model_helper.commons.AUGMENTATION, None)

    pass


if __name__ == "__main__":
    main()
