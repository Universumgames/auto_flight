import folium


def read_coordinates(filename):
    coords = []

    with open(filename, 'r') as file:
        for line in file:
            line = line.strip()
            if not line:
                continue

            parts = line.split(',')
            if len(parts) != 2:
                continue

            x, y = float(parts[0]), float(parts[1])

            # IMPORTANT: folium expects (lat, lon)
            coords.append((y, x))  # swap if your file is (x=lon, y=lat)

    return coords


def create_map(coords, output_file="map.html"):
    if not coords:
        print("No coordinates found.")
        return

    # Center map on first point
    m = folium.Map(location=coords[0], zoom_start=14)

    # Draw polyline
    folium.PolyLine(coords, color="blue", weight=3).add_to(m)

    # Optional: mark start/end
    folium.Marker(coords[0], tooltip="Start").add_to(m)
    folium.Marker(coords[-1], tooltip="End").add_to(m)

    # Save map
    m.save(output_file)
    print(f"Map saved to {output_file}")


if __name__ == "__main__":
    filename = "../flight_controller/sweepPath.txt"
    coords = read_coordinates(filename)
    create_map(coords)