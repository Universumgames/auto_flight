from tensorflow.keras import Sequential

import cli_defaults
import dataset_helper
import model_helper
from constants import DATASETS_DIR, EVALUATIONS_DIR
from dataset_helper import load, commons, splitter
from model_helper import trainModel, modelConverter, evaluate, commons


def test_model_iteration(slug: str, image_size: int, channel_count: int, batch_size: int, learning_rate: float,
                         epochs: int, augmentation: Sequential | None,
                         class_aliases: dict[str, list[str],] | None = None):
    DATASETS_DIR.mkdir(parents=True, exist_ok=True)

    train_ratio, val_ratio, test_ratio = cli_defaults.TRAIN_VAL_TEST_SPLIT
    seed = cli_defaults.SPLIT_SEED

    train_dir, val_dir, test_dir = splitter.download_and_split_dataset(slug, class_aliases, train_ratio, val_ratio,
                                                                       seed)

    train_ds = load.load_dataset(train_dir, image_size, batch_size, augmentation)
    val_ds = load.load_dataset(val_dir, image_size, batch_size, None)  # never augment validation data

    model_name = dataset_helper.commons.slug_to_id(slug)

    model, history = trainModel.train_new_model(model_name, channel_count, image_size, train_ds, val_ds,
                                                train_ds.class_names, learning_rate, epochs, None)

    trainModel.save_history(history, model_name)

    filenames = model_helper.commons.model_filenames(model_name)
    tflite_filename = filenames["tflite"]
    modelConverter.export_quantized(model, train_dir, image_size, tflite_filename)

    keras_filename = filenames["model"]

    metrics = evaluate.evaluate_models([keras_filename, tflite_filename], test_dir, image_size)

    evaluate.save_results(EVALUATIONS_DIR, metrics)
