import SharedLogic
import SwiftUI

struct AreaView: View {
    @Environment(ConnectionManager.self) private var connectionManager: ConnectionManager
    @Environment(AppState.self) private var appState

    @State private var polygon: [Coordinate] = []
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
                PolygonMapView(polygon: $polygon, basePosition: appState.basePosition, planePosition: appState.planes[defaultPlaneID]?.position, planeHeading: appState.planes[defaultPlaneID]?.heading)

                if loading {
                    ProgressView().padding().background(.thinMaterial).clipShape(RoundedRectangle(cornerRadius: 12))
                }
            }
            .clipShape(RoundedRectangle(cornerRadius: 12))
            .padding(.horizontal)
        }
        .navigationTitle(String(localized: "area.navTitle"))
        .padding(.bottom)
        .connectedToolbar()
        .task {
            let areaDefinition = await connectionManager.queryArea(DEFAULT_PLANE_ID)
            if let areaDefinition, polygon.isEmpty {
                polygon = areaDefinition.shape
            }
        }
    }

    private func submit() {
        guard !polygon.isEmpty, !loading else { return }
        loading = true
        connectionManager.submitArea(polygon: polygon) { result in
            loading = false
            if result == .ACCEPTED {
                appState.navigationPath.append(.ROUTE_APPROVAL)
            }
        }
    }
}

#Preview {
    VStack {
        NavigationStack {
            AreaView()
        }
        WizardProgressBar()
    }
    .environment(AppState.shared)
    .environment(ConnectionManager())
}
