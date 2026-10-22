# Entry point for PyInstaller (it can't build from a package's __main__.py with relative imports).
from ble_flight_bridge.cli import main

if __name__ == "__main__":
    main()
