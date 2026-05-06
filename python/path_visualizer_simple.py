import matplotlib.pyplot as plt


def read_coordinates(filename):
    x = []
    y = []

    with open(filename, 'r') as file:
        for line in file:
            line = line.strip()
            if not line:
                continue  # skip empty lines

            parts = line.split(',')
            if len(parts) != 2:
                continue  # skip malformed lines

            xi, yi = float(parts[0]), float(parts[1])
            x.append(xi)
            y.append(yi)

    return x, y


def plot_polyline(x, y):
    plt.figure()
    plt.plot(x, y, marker='o')  # markers help visualize points
    plt.xlabel("X")
    plt.ylabel("Y")
    plt.title("Polyline Plot")
    plt.grid(True)
    plt.axis('equal')  # keeps scale consistent
    plt.show()


if __name__ == "__main__":
    filename = "../flight_controller/sweepPath.txt"  # replace with your file path
    x, y = read_coordinates(filename)
    plot_polyline(x, y)