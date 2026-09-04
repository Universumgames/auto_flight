import SwiftUI

private struct ConnectedToolbar: ViewModifier {
    func body(content: Content) -> some View {
        content
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    ConnectionStatusButton()
                }
                ToolbarItem(placement: .topBarLeading) {
                    BaseDisconnectButton()
                }
            }
    }
}

extension View {
    func connectedToolbar() -> some View {
        modifier(ConnectedToolbar())
    }
}
