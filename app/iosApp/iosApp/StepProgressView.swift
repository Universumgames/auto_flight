import SwiftUI
import SharedLogic

/// Sidebar/step list mirroring `StepView.vue` - the six `ConfigurationState` steps
/// with a checkmark for completed steps and a spinner for the current one.
struct StepProgressView: View {
    @Environment(AppState.self) private var appState

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Flight Planner").font(.headline)
            ForEach(
                ConfigurationState.allCases
                    .enumerated()
                    .map { (index, step) in (index: index, translationKey: step.translationKey, step: step) }
                    .sorted(by: { $0.index < $1.index }),
                id: \.index
            ) { step in
                HStack(spacing: 8) {
                    Text(step.translationKey)
                    if appState.wizardStep.rawValue > step.index {
                        Image(systemName: "checkmark.circle.fill").foregroundStyle(.green)
                    } else if appState.wizardStep.rawValue == step.index {
                        ProgressView().scaleEffect(0.7).frame(width: 14, height: 14)
                    }
                    Spacer()
                }
                .font(.subheadline)
            }
        }
        .padding()
        .background(Color(.secondarySystemBackground))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }
}

#Preview {
    StepProgressView()
        .environment(AppState.shared)
}
