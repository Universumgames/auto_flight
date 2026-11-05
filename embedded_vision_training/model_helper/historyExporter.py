from tensorflow.keras.callbacks import History

from constants import HISTORY_DIR

def save_history(history: History, model_name: str):
    columns = list(dict.keys(history.history))
    steps = len(history.history[columns[0]])
    HISTORY_DIR.mkdir(parents=True, exist_ok=True)

    with open(HISTORY_DIR / f"history_{model_name}.csv", "w") as f:
        f.write(",".join(columns))
        f.write("\n")
        for step in range(0, steps):
            for key in columns:
                f.write(str(history.history[key][step]))
                if key is not columns[-1]:
                    f.write(",")
            f.write("\n")