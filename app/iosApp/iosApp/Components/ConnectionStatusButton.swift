import SwiftUI
import SharedLogic

/// Top-trailing connection status button that pops over the connection details.
struct ConnectionStatusButton: View {
    @Environment(AppState.self) private var appState
    @State private var showPopover = false

    var body: some View {
        Button {
            showPopover = true
        } label: {
            Image(systemName: appState.totalIsConnected ? "checkmark.circle.fill" : "arrow.triangle.2.circlepath")
                .font(.title2)
                .foregroundStyle(appState.totalIsConnected ? .green : .orange)
                .symbolEffect(.rotate, isActive: !appState.totalIsConnected)
        }
        .popover(isPresented: $showPopover) {
            ScrollView {
                ConnectionItemsList(items: [appState.baseStationItem] + appState.planeItems)
                    .padding()
            }
            .frame(minWidth: 300, minHeight: 200)
            .presentationCompactAdaptation(.popover)
        }
    }
}

#Preview {
    ConnectionStatusButton()
        .environment(AppState.shared)
}
