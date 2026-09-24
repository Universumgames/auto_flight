import SharedLogic
import SwiftUI

/// Mirrors `RoutePreviewView.vue`: poll for the planned route until the base
/// station has one ready, then show it alongside the live flight history.
struct RouteView: View {
    @Environment(ConnectionManager.self) private var connectionManager: ConnectionManager
    @Environment(AppState.self) private var appState

    @State private var plannedRoute: [Coordinate] = []
    @State private var pollTask: Task<Void, Never>?

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            ZStack {
                RouteMapView(
                    plannedRoute: plannedRoute,
                    flightHistory: appState.planes[defaultPlaneID]?.flightRoute ?? [],
                    basePosition: appState.basePosition,
                    planePosition: appState.planes[defaultPlaneID]?.position,
                    planeHeading: appState.planes[defaultPlaneID]?.heading ?? 0
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
        .connectedToolbar()
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
                let route = await connectionManager.queryRoute(DEFAULT_PLANE_ID)
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
            RouteView()
        }

        WizardProgressBar()
    }
    .environment(ConnectionManager())
    .environment(AppState.shared)
}
