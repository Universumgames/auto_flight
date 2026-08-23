import SwiftUI
import SharedLogic

struct BaseConnectedView: View {
    @Environment(ConnectionManager.self) private var connectionManager: ConnectionManager
    @Environment(AppState.self) private var appState

    var body: some View {
        NavigationStack(path: Bindable(appState).navigationPath) {
            ConnectionView()
                .navigationTitle(Text("Component Connections"))
                .toolbar {
                    ToolbarItem(placement: .topBarTrailing) {
                        ConnectionStatusButton()
                    }
                    ToolbarItem(placement: .topBarLeading) {
                        BaseDisconnectButton()
                    }
                }
                .navigationDestination(for: ConfigurationState.self) { state in
                    switch state {
                    case .AREA_SELECTION:
                        AreaView()
                    case .ROUTE_APPROVAL:
                        RouteView()
                    default:
                        EmptyView()
                    }
                }
        }
        .safeAreaInset(edge: .top, spacing: 0) {
            WizardProgressBar()
        }
    }
}

#Preview {
    BaseConnectedView()
        .environment(ConnectionManager())
        .environment(AppState.shared)
}
