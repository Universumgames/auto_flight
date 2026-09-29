import SharedLogic
import SwiftUI

struct ConnectionView: View {
    @Environment(ConnectionManager.self) private var connectionManager: ConnectionManager
    @Environment(AppState.self) private var appState

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                ConnectionItemList(item: appState.baseStationItem) { EmptyView() }

                ForEach(
                    appState.planes
                        .sorted(
                            by: { (
                                $0.value.wizardStep.last ?? .CONNECTING
                            ) > ($1.value.wizardStep.last ?? .CONNECTING)
                            }),
                    id: \.key
                ) {
                    pair in
                    ConnectionItemList(item: pair.value.connectionItem) {
                        WizardProgressBar(
                            wizardStep: pair.value.wizardStep.last
                                ?? .CONNECTION)
                        Button(String(localized: "connection.btn.nextStep")) {
                            appState.navigationPath.append(pair.key)
                        }
                        .buttonStyle(.borderedProminent)
                        .disabled(!appState.totalIsConnected)
                        .padding(.top, 8)
                    }
                }

                if appState.planeItems.isEmpty {
                    Text("connection.planes.empty")
                }
            }
            .padding()
        }
        .connectedToolbar(planeId: nil)
    }
}

#Preview {
    ConnectionView()
        .environment(ConnectionManager())
        .environment(AppState.preview)
}
