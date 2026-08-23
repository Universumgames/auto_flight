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

    @State private var host = ""

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 8) {
                Text("Connection Overview").font(.title2).bold()
                Text("Enter the base station's address on your local network, then wait for every link to come up.")
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
            }

            HStack {
                TextField("Base station host, e.g. 192.168.4.1", text: $host)
                    .textFieldStyle(.roundedBorder)
                    .autocorrectionDisabled()
                #if os(iOS)
                    .textInputAutocapitalization(.never)
                    .keyboardType(.URL)
                #endif
                Button("Connect") {
                    connectionManager.connect(host: host)
                }
                .buttonStyle(.borderedProminent)
                .disabled(host.isEmpty)
            }
        }
        .padding()
        .onAppear {
            host = connectionManager.host
        }
    }
}

#Preview {
    BaseSelectionView()
        .environment(ConnectionManager())
        .environment(AppState.shared)
}
