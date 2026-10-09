import SharedLogic
import SwiftUI

/// Single flat route type for `BaseConnectedView`'s one `NavigationStack`.
///
/// Nesting a second `NavigationStack` inside a `navigationDestination(for:)`
/// closure doesn't work on iOS for push-style navigation: both stacks end up
/// sharing the same underlying `UINavigationController` push list, but SwiftUI
/// tracks them as two separately-typed path bindings (`[PlaneID]` outer,
/// `[ConfigurationState]` inner) and crashes reconciling them
/// (`AnyNavigationPath.Error.comparisonTypeMismatch`). Flattening both into one
/// path of one type avoids that entirely.
private enum ConnectedRoute: Hashable {
    case plane(PlaneID)
    case configuration(PlaneID, ConfigurationState)
}

struct PlaneSelectionView: View {
    @Environment(ConnectionManager.self) private var connectionManager: ConnectionManager
    @Environment(AppState.self) private var appState

    /// Combines `appState.navigationPath` (which plane is selected) with that
    /// plane's own `wizardStep` (its progress through the wizard) into one path,
    /// and splits edits back into those two underlying sources of truth so each
    /// plane keeps its own wizard progress independently.
    private var routePath: Binding<[ConnectedRoute]> {
        Binding(
            get: {
                var path = appState.navigationPath.map { ConnectedRoute.plane($0) }
                if let planeID = appState.navigationPath.last, let plane = appState.planes[planeID] {
                    path += plane.wizardStep.map { ConnectedRoute.configuration(planeID, $0) }
                }
                return path
            },
            set: { newPath in
                var planeIDs: [PlaneID] = []
                var wizardSteps: [ConfigurationState] = []
                for route in newPath {
                    switch route {
                    case let .plane(id):
                        planeIDs.append(id)
                    case let .configuration(_, state):
                        wizardSteps.append(state)
                    }
                }
                appState.navigationPath = planeIDs
                if let planeID = planeIDs.last {
                    appState.planes[planeID]?.wizardStep = wizardSteps
                }
            }
        )
    }

    @ViewBuilder
    var planesDetails: some View {
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
            ConnectionItemList(item: pair.value.connectionItem, collapsable: true) { collapsed in
                WizardProgressBar(
                    wizardStep: ConfigurationState.Companion.shared
                        .fromFlightState(flightState: pair.value.flightState))
                if !collapsed {
                    Button(String(localized: "connection.btn.nextStep")) {
                        pair.value.wizardStep = [ConfigurationState.Companion.shared
                            .fromFlightState(flightState: pair.value.flightState)]
                        appState.navigationPath.append(pair.key)
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(!appState.totalIsConnected)
                    .padding(.top, 8)
                }
            }
        }
    }

    var body: some View {
        NavigationStack(path: routePath) {
            ScrollView {
                VStack(alignment: .leading, spacing: 16) {
                    ConnectionItemList(item: appState.baseStationItem, collapsable: false) { _ in
                        EmptyView()
                    }

                    planesDetails

                    if appState.planeItems.isEmpty {
                        Text("connection.planes.empty")
                    }
                }
                .padding()
            }
            .connectedToolbar(planeId: nil)
            .navigationTitle(Text(String(localized: "connection.navTitle")))
            .navigationDestination(for: ConnectedRoute.self) { route in
                switch route {
                case let .plane(planeID):
                    AreaView(planeId: planeID)
                case let .configuration(planeID, state):
                    switch state {
                    case .AREA_SELECTION:
                        AreaView(planeId: planeID)
                    case .ROUTE_APPROVAL:
                        RouteView(planeId: planeID)
                    default:
                        EmptyView()
                    }
                }
            }
        }
    }
}

#Preview {
    PlaneSelectionView()
        .environment(ConnectionManager())
        .environment(AppState.shared)
}
