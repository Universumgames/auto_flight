import SharedLogic
import SwiftUI

/// Top-trailing connection status button that pops over the connection details.
struct ConnectionStatusButton: View {
    let planeId: PlaneID?
    @Environment(AppState.self) private var appState
    @State private var showPopover = false

    func totalIsConnected(plane: PlaneInfo) -> Bool {
        return appState.baseStationItem.status == .CONNECTED && plane.connectionItem
            .status == .CONNECTED
    }

    var body: some View {
        Button {
            showPopover = true
        } label: {
            Text(String(localized: "connection.status.label"))
            Image(systemName: appState.totalIsConnected ? "checkmark.circle.fill" : "arrow.triangle.2.circlepath")
                .font(.title2)
                .foregroundStyle(appState.totalIsConnected ? .green : .orange)
                .symbolEffect(.rotate, isActive: !appState.totalIsConnected)
        }
        .popover(isPresented: $showPopover) {
            ScrollView {
                VStack(alignment: .leading, spacing: 12) {
                    ConnectionItemList(
                        item: appState.baseStationItem,
                        collapsable: false
                    ) { _ in
                        EmptyView()
                    }

                    if let planeId, let plane = appState
                        .planes[planeId] {
                        ConnectionItemList(
                            item: plane.connectionItem, collapsable: false) { _ in
                                EmptyView()
                            }
                    }
                }
                .padding()
            }
            .frame(minWidth: 300, minHeight: 200)
            .presentationCompactAdaptation(.popover)
        }
    }
}

#Preview {
    ConnectionStatusButton(planeId: defaultPlaneID)
        .environment(AppState.shared)
}
