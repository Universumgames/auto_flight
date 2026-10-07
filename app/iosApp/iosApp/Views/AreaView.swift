import SharedLogic
import SwiftUI

struct AreaView: View {
    let planeId: PlaneID

    @Environment(ConnectionManager.self) private var connectionManager: ConnectionManager
    @Environment(AppState.self) private var appState

    @State private var polygon: [Coordinate] = []
    @State private var routeSettings: SwiftRouteSettings = .standard
    @State private var loading = false
    @State private var settingsOpen = false

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

            VStack(alignment: .leading, spacing: 12) {
                Section(isExpanded: $settingsOpen) {
                    VStack {
                        LabeledContent {
                            Picker(
                                "area.settings.algorithm",
                                selection: $routeSettings.routeAlgorithm
                            ) {
                                ForEach(wire.RouteAlgorithm.allCases, id: \.self) { algorithm in
                                    Text(
                                        algorithm.description
                                            .lowercased().capitalized
                                    )
                                    .tag(algorithm)
                                }
                            }
                        } label: {
                            Text("area.settings.algorithm")
                        }
                        LabeledContent {
                            HStack {
                                Slider(
                                    value: Binding(
                                        get: { Double(routeSettings.overlapPercentage)
                                        },
                                        set: {
                                            routeSettings.overlapPercentage = Int($0)
                                        }
                                    ),
                                    in: 0 ... 100,
                                    step: 1
                                )
                                Text("\(routeSettings.overlapPercentage)%")
                                    .frame(width: 40, alignment: .leading)
                            }
                        } label: {
                            Text("area.settings.overlap")
                        }
                    }
                } header: {
                    Button {
                        withAnimation {
                            settingsOpen.toggle()
                        }
                    } label: {
                        HStack{
                            Text("area.settings")
                            Spacer()
                            Image(systemName: settingsOpen ? "chevron.up" : "chevron.down")
                                .foregroundStyle(.secondary)
                        }
                        .contentShape(Rectangle())
                    }
                    .buttonStyle(.plain)
                    
                }
            }
            .padding()
            .background(Color(.secondarySystemBackground))
            .clipShape(RoundedRectangle(cornerRadius: 10))
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
                routeSettings: routeSettings.toWire()
            ) { result in
                loading = false
                if result == .ACCEPTED {
                    appState
                        .planes[planeId]?.area = AreaData(
                            areaPoints: polygon,
                            settings: routeSettings.toWire()
                        )
                    appState.planes[planeId]?.wizardStep.append(.ROUTE_APPROVAL)
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
