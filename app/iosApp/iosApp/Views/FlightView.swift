import SharedLogic
import SwiftUI

/// Live flight screen shown after the route was approved: the planned route and
/// area boundary stay visible while `FlightMapView` traces the plane's position.
struct FlightView: View {
    let planeId: PlaneID
    @Environment(ConnectionManager.self) private var connectionManager: ConnectionManager
    @Environment(AppState.self) private var appState

    @State private var pollTask: Task<Void, Never>?

    private var plannedRoute: [Coordinate] {
        appState.planes[planeId]?.plannedRoute ?? []
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                WizardProgressBar(wizardStep: .FLYING)
                Spacer()
            }
            .padding(.horizontal)
            ZStack {
                FlightMapView(
                    plannedRoute: plannedRoute,
                    positionHistory: appState.planes[planeId]?.positionHistory ?? [],
                    areaPolygon: appState
                        .planes[planeId]?.area?.areaPoints ?? [],
                    basePosition: appState.basePosition,
                    planePosition: appState.planes[planeId]?.position,
                    planeHeading: appState.planes[planeId]?.heading ?? 0
                )
            }
            .frame(minHeight: 400, maxHeight: .infinity)
            .clipShape(RoundedRectangle(cornerRadius: 12))
            .padding(.horizontal)

            if plannedRoute.isEmpty {
                HStack {
                    ProgressView()
                    Text(String(localized: "route.waitingForRoute")).foregroundStyle(.secondary)
                }
                .padding(.horizontal)
            }
        }
        .navigationTitle(String(localized: "flight.navTitle"))
        .padding(.bottom)
        .connectedToolbar(planeId: planeId)
        .onAppear {
            startPolling()
        }
        .onDisappear {
            pollTask?.cancel()
        }
    }

    /// The route is normally already stored by `RouteView`; only query it when
    /// this screen is reached without it (e.g. after reconnecting mid-flight).
    private func startPolling() {
        pollTask?.cancel()
        pollTask = Task {
            var tries = 0
            while !Task.isCancelled && plannedRoute.isEmpty && tries < 50 {
                let route = await connectionManager.queryRoute(planeId)
                if let route, !route.route.isEmpty {
                    appState.planes[planeId]?.plannedRoute = route.route
                    return
                }
                tries += 1
                try? await Task.sleep(nanoseconds: 1000000000)
            }
        }
    }
}

#Preview {
    NavigationStack {
        FlightView(planeId: DEFAULT_PLANE_ID)
    }
    .environment(ConnectionManager())
    .environment(AppState.shared)
}
