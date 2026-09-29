import SharedLogic
import SwiftUI

struct AreaView: View {
    let planeId: PlaneID
    
    @Environment(ConnectionManager.self) private var connectionManager: ConnectionManager
    @Environment(AppState.self) private var appState

    @State private var polygon: [Coordinate] = []
    @State private var routeSettings: wire.RouteSettings = .init(
        routeAlgorithm: .BASIC,
        overlapPercentage: 20
    )
    @State private var loading = false

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                VStack(alignment: .leading, spacing: 4) {
                    Text(String(localized: "area.instructions")).font(.subheadline).foregroundStyle(.secondary)
                }
                Spacer()
                Button(String(localized: "area.btn.nextStep")) {
                    submit()
                }
                .buttonStyle(.borderedProminent)
                .disabled(polygon.isEmpty || loading)
                .padding(.horizontal)
            }
            .padding(.horizontal)

            ZStack {
                PolygonMapView(
                    polygon: $polygon,
                    basePosition: appState.basePosition,
                    planePosition: appState.planes[planeId]?.position,
                    planeHeading: appState.planes[planeId]?.heading
                )

                if loading {
                    ProgressView().padding().background(.thinMaterial).clipShape(RoundedRectangle(cornerRadius: 12))
                }
            }
            .clipShape(RoundedRectangle(cornerRadius: 12))
            .padding(.horizontal)
        }
        .navigationTitle(String(localized: "area.navTitle"))
        .padding(.bottom)
        .connectedToolbar(planeId: planeId)
        .task {
            loading = true
            let areaDefinition = await connectionManager.queryArea(planeId)
            if let areaDefinition {
                polygon = areaDefinition.shape
            }
            loading = false
        }
    }

    private func submit() {
        guard !polygon.isEmpty, !loading else { return }
        loading = true
        connectionManager
            .submitArea(
                planeId: planeId,
                polygon: polygon,
                routeSettings: routeSettings
            ) { result in
            loading = false
            if result == .ACCEPTED {
                appState
                    .planes[planeId]?.area = AreaData(
                        areaPoints: polygon,
                        settings: routeSettings
                    )
                appState.planes[planeId]!.wizardStep.append(.ROUTE_APPROVAL)
            }
        }
    }
}

#Preview {
    VStack {
        NavigationStack {
            AreaView(planeId: DEFAULT_PLANE_ID)
        }
        WizardProgressBar(wizardStep: .AREA_SELECTION)
    }
    .environment(AppState.shared)
    .environment(ConnectionManager())
}
