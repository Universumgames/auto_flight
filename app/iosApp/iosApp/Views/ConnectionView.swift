import SharedLogic
import SwiftUI

struct ConnectionView: View {
    @Environment(ConnectionManager.self) private var connectionManager: ConnectionManager
    @Environment(AppState.self) private var appState

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                ConnectionItemsList(items: [appState.baseStationItem] + appState.planeItems)

                Button("Next Step") {
                    appState.navigationPath.append(.AREA_SELECTION)
                }
                .buttonStyle(.borderedProminent)
                .disabled(!appState.totalIsConnected)
                .padding(.top, 8)
            }
            .padding()
        }
        .connectedToolbar()
    }
}

#Preview {
    ConnectionView()
        .environment(ConnectionManager())
        .environment(AppState.shared)
}
