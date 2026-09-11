import SwiftUI

@main
struct iOSApp: App {
    @State private var connectionManager = ConnectionManager()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environment(connectionManager)
                .environment(AppState.shared)
        }
    }
}
