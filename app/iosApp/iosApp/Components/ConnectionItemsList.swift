import SharedLogic
import SwiftUI

/// Shared list rendering for `ConnectionItems.shared.items(state:)`, used both by
/// the full Connection screen and the compact status popover.
struct ConnectionItemsList: View {
    let items: [ConnectionItem]

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            ForEach(Array(items.enumerated()), id: \.offset) { _, item in
                ConnectionItemList(item: item){EmptyView()}
            }
        }
    }
}

struct ConnectionItemList<Content: View>: View {
    let item: ConnectionItem
    @ViewBuilder let bottomAppendView: () -> Content

    @ViewBuilder
    func batteryIcon(_ percent: Int) -> some View {
        switch percent {
        case 0 ... 20:
            Image(systemName: "battery.0percent").foregroundStyle(.red)
        case 21 ... 40:
            Image(systemName: "battery.25percent").foregroundStyle(.orange)
        case 41 ... 60:
            Image(systemName: "battery.50percent").foregroundStyle(.yellow)
        case 61 ... 80:
            Image(systemName: "battery.75percent").foregroundStyle(.green)
        case 81 ... 100:
            Image(systemName: "battery.100percent").foregroundStyle(.green)
        default:
            Image(systemName: "battery.0percent").foregroundStyle(.gray)
        }
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                StatusDot(connected: ConnectionItems.shared.isConnected(status: item.status))
                Text(item.label).font(.headline)
                batteryIcon(Int(item.batteryPercent))
                Text(
                    String(
                        localized: "connection.items.batteryPercent \(item.batteryPercent)"
                    )
                )
                .font(.subheadline)
                .foregroundStyle(.secondary)
                Spacer()
                Text(ConnectionItems.shared.formatStatus(status: item.status))
                    .font(.caption)
                    .padding(.horizontal, 8)
                    .padding(.vertical, 2)
                    .background(Color.secondary.opacity(0.15))
                    .clipShape(Capsule())
            }
            ForEach(Array(item.subTasks.enumerated()), id: \.offset) { _, subTask in
                HStack(spacing: 6) {
                    if subTask.state == .DONE {
                        Image(systemName: "checkmark.circle.fill").foregroundStyle(.green)
                    } else {
                        ProgressView().scaleEffect(0.6).frame(width: 14, height: 14)
                    }
                    Text(subTask.label).font(.subheadline).foregroundStyle(.secondary)
                }
            }
            VStack{
                bottomAppendView()
            }
        }
        .padding(12)
        .background(Color(.secondarySystemBackground))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }
}

struct StatusDot: View {
    let connected: Bool
    var body: some View {
        if connected {
            Image(systemName: "checkmark.circle.fill").foregroundStyle(.green)
        } else {
            ProgressView().scaleEffect(0.7).frame(width: 16, height: 16)
        }
    }
}

#Preview {
    ConnectionItemsList(items: [AppState.shared.baseStationItem] + AppState.shared.planeItems)
}
