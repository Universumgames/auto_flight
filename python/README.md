# Path Visualizers

Two small scripts for visualizing flight paths stored as CSV coordinate files (e.g. `../flight_controller/sweepPath.txt`).

## Input format

Both scripts expect a plain text file with one point per line, as `x,y` (comma-separated floats):

```
0.0,0.0
0.0,10.0
8.5,11.6
...
```

Empty lines and malformed lines (not exactly two comma-separated values) are skipped.

## Setup

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

## `path_visualizer_simple.py`

Plots the path as a 2D polyline using `matplotlib`. Useful for a quick local look at raw x/y coordinates (e.g. planner output in meters/local frame).

```bash
python3 path_visualizer_simple.py
```

Opens a matplotlib window with the polyline plotted, points marked, and equal-scale axes.

## `path_visualizer_map.py`

Renders the path on an interactive OpenStreetMap-based map using `folium`. Useful when the coordinates are geographic (lat/lon).

```bash
python3 path_visualizer_map.py
```

Note: the script swaps the columns when reading, treating the file as `(lon, lat)` and converting to the `(lat, lon)` order folium expects. Adjust the swap in `read_coordinates` if your file is already `(lat, lon)`.

Generates `map.html` in the current directory — open it in a browser to view the path, with markers for the start and end points.

## Configuring the input file

Both scripts hardcode the input path at the bottom of the file:

```python
filename = "../flight_controller/sweepPath.txt"
```

Edit this line to point at a different coordinate file.
