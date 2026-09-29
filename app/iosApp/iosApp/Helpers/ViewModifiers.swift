import SwiftUI
import SharedLogic

private struct ConnectedToolbar: ViewModifier {
    let planeId: PlaneID?
    func body(content: Content) -> some View {
        content
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    ConnectionStatusButton(planeId: planeId)
                }
                ToolbarItem(placement: .topBarLeading) {
                    BaseDisconnectButton()
                }
            }
    }
}

extension View {
    func connectedToolbar(planeId: PlaneID?) -> some View {
        modifier(ConnectedToolbar(planeId: planeId))
    }
}
