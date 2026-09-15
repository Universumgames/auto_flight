import SwiftUI
import SharedLogic

/// Error/warning banner shown below the top bar, matching `App.vue`'s precedence:
/// motor controller disconnected > GPS unavailable (suppressed on the connection
/// screen) > autopilot manual override.
struct WarningBanner: View {
    @Environment(AppState.self) private var appState
    /// Suppresses the GPS-unavailable banner while the user is still on the connection screen.
    var suppressGpsWarning: Bool = false

    var body: some View {
        if appState.motorControllerDisconnectedError {
            banner(text: String(localized: "warning.motorControllerDisconnected"), systemImage: "xmark.circle.fill", color: .red)
        } else if !suppressGpsWarning && appState.gpsPlaneUnavailableError {
            banner(text: String(localized: "warning.gpsUnavailable"), systemImage: "xmark.circle.fill", color: .red)
        } else if appState.autopilotDisabledWarning {
            banner(text: String(localized: "warning.autopilotDisabled"), systemImage: "exclamationmark.triangle.fill", color: .orange)
        }
    }

    private func banner(text: String, systemImage: String, color: Color) -> some View {
        HStack(spacing: 8) {
            Image(systemName: systemImage).foregroundStyle(color)
            Text(text).font(.subheadline).fontWeight(.semibold)
        }
        .padding(.horizontal, 14)
        .padding(.vertical, 8)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(color.opacity(0.12))
    }
}
