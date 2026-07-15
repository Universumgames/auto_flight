# Bridge defaults. Edit here to change without passing CLI flags every time;
# all values can still be overridden with the matching --flag.

DEFAULT_BAUD_RATE = 115200
DEFAULT_WS_HOST = "localhost"
DEFAULT_WS_PORT = 8765
DEFAULT_HTTP_HOST = "localhost"
DEFAULT_HTTP_PORT = 8000

# Seconds to wait before retrying after the serial connection drops
# (device unplugged, port error, etc.).
RECONNECT_DELAY_SECONDS = 2.0
