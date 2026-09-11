//
//  BaseConnectionView.swift
//  iosApp
//
//  Created by Tom Arlt on 21.08.26.
//

import SwiftUI

struct BaseSelectionView: View {
    @Environment(ConnectionManager.self) private var connectionManager: ConnectionManager
    @Environment(AppState.self) private var appState

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 8) {
                    HStack {
                        Text("Available Base Stations")
                            .font(.headline)
                        Spacer()
                        if connectionManager.isScanning {
                            ProgressView().scaleEffect(0.8)
                        }
                        Button(connectionManager.isScanning ? "Stop" : "Scan") {
                            if connectionManager.isScanning {
                                connectionManager.stopBluetoothScan()
                            } else {
                                connectionManager.startBluetoothScan()
                            }
                        }
                        .buttonStyle(.bordered)
                    }

                    if connectionManager.bluetoothState == .poweredOff {
                        Text("Bluetooth is off — enable it in Settings.")
                            .font(.caption)
                            .foregroundStyle(.red)
                    } else if connectionManager.discoveredBaseStations.isEmpty {
                        Text(connectionManager.isScanning ? "Searching…" : "Tap Scan to find nearby devices.")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    } else {
                        ForEach(connectionManager.discoveredBaseStations) { station in
                            HStack {
                                Image(systemName: "antenna.radiowaves.left.and.right")
                                VStack(alignment: .leading) {
                                    Text(station.name)
                                    Text("Build \(station.buildDate?.formatted(date: .abbreviated, time: .shortened) ?? "Unkown")")
                                        .font(.caption2)
                                        .foregroundStyle(.secondary)
                                }
                                Spacer()
                                Button {
                                    connectionManager.tryConnectToPeripheral(station.peripheral)
                                } label: {
                                    Text("Connect")
                                }
                                .disabled(
                                    appState.connectionStateBaseStation == .CONNECTING
                                )
                            }
                            .padding(.vertical, 4)
                            Divider()
                        }
                    }
                }
                .padding()
                .background(Color(.secondarySystemBackground))
                .clipShape(RoundedRectangle(cornerRadius: 10))
            }
            .navigationTitle("Connect to a Base Station")
            .padding()
        }
        .onAppear {
            connectionManager.startBluetoothScan()
        }
    }
}

#Preview {
    BaseSelectionView()
        .environment(ConnectionManager())
        .environment(AppState.shared)
}
