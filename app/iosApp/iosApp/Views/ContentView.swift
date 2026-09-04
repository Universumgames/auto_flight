import SwiftUI

struct ContentView: View {
    @Environment(ConnectionManager.self) private var connectionManager: ConnectionManager
    @Environment(AppState.self) private var appState

    var body: some View {
        Group {
            if appState.connectionStateBaseStation == .DISCONNECTED || appState.connectionStateBaseStation == .CONNECTING {
                BaseSelectionView()
            } else {
                BaseConnectedView()
                    .onAppear { appState.resetNavigation() }
            }
        }
    }
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView()
            .environment(ConnectionManager())
            .environment(AppState.shared)
    }
}
