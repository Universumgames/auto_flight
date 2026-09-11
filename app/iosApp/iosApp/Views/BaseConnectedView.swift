import SwiftUI
import SharedLogic

struct BaseConnectedView: View {
    @Environment(ConnectionManager.self) private var connectionManager: ConnectionManager
    @Environment(AppState.self) private var appState

    var body: some View {
        VStack(spacing: 0) {
            NavigationStack(path: Bindable(appState).navigationPath) {
                ConnectionView()
                    .navigationTitle(Text("Component Connections"))
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

            WizardProgressBar()
        }
    }
}

#Preview {
    BaseConnectedView()
        .environment(ConnectionManager())
        .environment(AppState.shared)
}
