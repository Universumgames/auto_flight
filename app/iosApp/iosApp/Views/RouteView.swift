import SharedLogic
import SwiftUI

/// Mirrors `RoutePreviewView.vue`: poll for the planned route until the base
/// station has one ready, then show it alongside the live flight history.
struct RouteView: View {
    let planeId: PlaneID
    @Environment(ConnectionManager.self) private var connectionManager: ConnectionManager
    @Environment(AppState.self) private var appState

    @State private var plannedRoute: [Coordinate] = []
    @State private var pollTask: Task<Void, Never>?

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            ZStack {
                RouteMapView(
                    plannedRoute: plannedRoute,
                    flightHistory: appState.planes[planeId]?.flightRoute ?? [],
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
        .navigationTitle(String(localized: "route.navTitle"))
        .padding(.bottom)
        .connectedToolbar(planeId: planeId)
        .onAppear {
            startPolling()
        }
        .onDisappear {
            pollTask?.cancel()
        }
    }

    private func startPolling() {
        pollTask?.cancel()
        pollTask = Task {
            var tries = 0
            while !Task.isCancelled && plannedRoute.isEmpty && tries < 50 {
                let route = await connectionManager.queryRoute(planeId)
                if let route, !route.isEmpty {
                    plannedRoute = route
                    return
                }
                tries += 1
                try? await Task.sleep(nanoseconds: 1000000000)
            }
        }
    }
}

#Preview {
    VStack {
        NavigationStack {
            RouteView(planeId: DEFAULT_PLANE_ID)
        }

        WizardProgressBar(wizardStep: .ROUTE_APPROVAL)
    }
    .environment(ConnectionManager())
    .environment(AppState.shared)
}
