import SwiftUI
import SharedLogic

/// Top-trailing connection status button that pops over the connection details.
struct BaseDisconnectButton: View {
    @Environment(ConnectionManager.self) private var connectionManager: ConnectionManager
    @Environment(AppState.self) private var appState
    @State private var showPopover = false

    var body: some View {
        Button(role: .destructive) {
            showPopover = true
        } label: {
            Image(systemName: "x.circle.fill")
                .foregroundStyle(.red)
        }
        .popover(isPresented: $showPopover) {
            Button(role:.destructive){
                connectionManager.disconnect()
            } label: {
                Text(String(localized: "connection.disconnect.confirm"))
            }
            .padding()
            .presentationCompactAdaptation(.popover)
        }
    }
}

#Preview {
    BaseDisconnectButton()
        .environment(ConnectionManager())
        .environment(AppState.shared)
}
