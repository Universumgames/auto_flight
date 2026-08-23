import SwiftUI
import SharedLogic

private extension ConfigurationState {
    var displayLabel: String {
        switch self {
        case .CONNECTION: return "Connect"
        case .AREA_SELECTION: return "Area"
        case .ROUTE_APPROVAL: return "Route"
        case .STARTING: return "Starting"
        case .FLYING: return "Flying"
        case .FINISHING: return "Done"
        default: return translationKey
        }
    }
}

struct WizardProgressBar: View {
    @Environment(AppState.self) private var appState

    private var steps: [ConfigurationState] {
        ConfigurationState.allCases
            .filter { $0.index >= 0 }
            .sorted { $0.index < $1.index }
    }

    var body: some View {
        let currentIdx = Int(appState.wizardStep.index)
        HStack(alignment: .top, spacing: 0) {
            ForEach(Array(steps.enumerated()), id: \.offset) { i, step in
                let idx = Int(step.index)
                VStack(spacing: 4) {
                    HStack(spacing: 0) {
                        if i > 0 {
                            Rectangle()
                                .fill(idx <= currentIdx ? Color.accentColor : Color.secondary.opacity(0.3))
                                .frame(maxWidth: .infinity, maxHeight: 2)
                        } else {
                            Color.clear.frame(maxWidth: .infinity, maxHeight: 2)
                        }

                        stepCircle(index: idx, currentIndex: currentIdx)

                        if i < steps.count - 1 {
                            Rectangle()
                                .fill(idx < currentIdx ? Color.accentColor : Color.secondary.opacity(0.3))
                                .frame(maxWidth: .infinity, maxHeight: 2)
                        } else {
                            Color.clear.frame(maxWidth: .infinity, maxHeight: 2)
                        }
                    }

                    Text(step.displayLabel)
                        .font(.system(size: 9, weight: idx == currentIdx ? .semibold : .regular))
                        .foregroundStyle(idx <= currentIdx ? .primary : .secondary)
                        .multilineTextAlignment(.center)
                }
                .frame(maxWidth: .infinity)
            }
        }
        .padding(.horizontal, 16)
        .padding(.vertical, 10)
        .background(.bar)
    }

    @ViewBuilder
    private func stepCircle(index: Int, currentIndex: Int) -> some View {
        ZStack {
            Circle()
                .fill(circleColor(index: index, currentIndex: currentIndex))
                .frame(width: 24, height: 24)
            if index < currentIndex {
                Image(systemName: "checkmark")
                    .font(.system(size: 10, weight: .bold))
                    .foregroundStyle(.white)
            } else {
                Text("\(index + 1)")
                    .font(.system(size: 11, weight: .semibold))
                    .foregroundStyle(index == currentIndex ? .white : Color.secondary)
            }
        }
    }

    private func circleColor(index: Int, currentIndex: Int) -> Color {
        if index < currentIndex { return .green }
        if index == currentIndex { return .accentColor }
        return Color(.systemFill)
    }
}

#Preview {
    WizardProgressBar()
        .environment(AppState.shared)
}
