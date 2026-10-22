// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "BLERecorder",
    platforms: [.macOS(.v13)],
    targets: [
        .executableTarget(
            name: "ble-recorder",
            path: "Sources/BLERecorder",
            exclude: ["Info.plist"],
            linkerSettings: [
                // CoreBluetooth requires NSBluetoothAlwaysUsageDescription even for a bare CLI;
                // without an embedded Info.plist, TCC kills the process on first BLE use.
                .unsafeFlags([
                    "-Xlinker", "-sectcreate",
                    "-Xlinker", "__TEXT",
                    "-Xlinker", "__info_plist",
                    "-Xlinker", "Sources/BLERecorder/Info.plist",
                ]),
            ]
        ),
    ]
)
