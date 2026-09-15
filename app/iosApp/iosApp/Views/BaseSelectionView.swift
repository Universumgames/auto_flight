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
                        Text(String(localized: "setup.baseScan.title"))
                            .font(.headline)
                        Spacer()
                        if connectionManager.isScanning {
                            ProgressView().scaleEffect(0.8)
                        }
                        Button(connectionManager.isScanning
                               ? String(localized: "setup.baseScan.btn.stop")
                               : String(localized: "setup.baseScan.btn.scan")) {
                            if connectionManager.isScanning {
                                connectionManager.stopBluetoothScan()
                            } else {
                                connectionManager.startBluetoothScan()
                            }
                        }
                        .buttonStyle(.bordered)
                    }

                    if connectionManager.bluetoothState == .poweredOff {
                        Text(String(localized: "setup.baseScan.bluetoothOff"))
                            .font(.caption)
                            .foregroundStyle(.red)
                    } else if connectionManager.discoveredBaseStations.isEmpty {
                        Text(connectionManager.isScanning
                             ? String(localized: "setup.baseScan.searching")
                             : String(localized: "setup.baseScan.tapToScan"))
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    } else {
                        ForEach(connectionManager.discoveredBaseStations) { station in
                            HStack {
                                Image(systemName: "antenna.radiowaves.left.and.right")
                                VStack(alignment: .leading) {
                                    Text(station.name)
                                    Text(String(localized: "setup.baseScan.buildDate"))
                                        .font(.caption2)
                                        .foregroundStyle(.secondary)
                                }
                                Spacer()
                                Button {
                                    connectionManager.tryConnectToPeripheral(station.peripheral)
                                } label: {
                                    Text(String(localized: "setup.baseScan.btn.connect"))
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
            .navigationTitle(String(localized: "setup.baseScan.navTitle"))
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
