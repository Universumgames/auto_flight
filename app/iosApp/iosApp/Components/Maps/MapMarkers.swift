import SwiftUI

/// Small colored circular badge mirroring the web app's custom Leaflet `divIcon`
/// markers for the base station ("BS") and plane ("PL").
struct MarkerBadge: View {
    private enum Metrics {
        static let diameter: CGFloat = 28
        static let strokeWidth: CGFloat = 2
        static let shadowRadius: CGFloat = 2
        static let strokeColor: Color = .white
        static let labelFontSize: CGFloat = 11
        static let labelFontWeight: Font.Weight = .bold
        static let labelColor: Color = .white
        static let coneLength: CGFloat = 55
        static let coneHalfAngle: Double = 22
        static let coneInnerOpacity: Double = 0.55
    }

    let text: String
    let color: Color
    var rotation: Double? = nil

    var body: some View {
        ZStack {
            if let rotation {
                DirectionCone(halfAngle: Metrics.coneHalfAngle)
                    .fill(
                        RadialGradient(
                            colors: [color.opacity(Metrics.coneInnerOpacity), color.opacity(0)],
                            center: .center,
                            startRadius: Metrics.diameter / 2,
                            endRadius: Metrics.coneLength
                        )
                    )
                    .frame(width: Metrics.coneLength * 2, height: Metrics.coneLength * 2)
                    .rotationEffect(.degrees(rotation))
            }
            Circle()
                .fill(color)
                .frame(width: Metrics.diameter, height: Metrics.diameter)
                .overlay(Circle().stroke(Metrics.strokeColor, lineWidth: Metrics.strokeWidth))
                .shadow(radius: Metrics.shadowRadius)
            Text(text)
                .font(.system(size: Metrics.labelFontSize, weight: Metrics.labelFontWeight))
                .foregroundStyle(Metrics.labelColor)
        }
    }
}

/// Shared label/color presets so marker styling isn't repeated at each call site.
extension MarkerBadge {
    enum Preset {
        static let baseStationText = "BS"
        static let baseStationColor = Color.blue
        static let planeText = "PL"
        static let planeColor = Color.orange
        static let routeStartText = "S"
        static let routeStartColor = Color.green
        static let routeEndText = "E"
        static let routeEndColor = Color.red
    }

    static func baseStation() -> MarkerBadge {
        MarkerBadge(text: Preset.baseStationText, color: Preset.baseStationColor)
    }

    static func plane(rotation: Double? = nil) -> MarkerBadge {
        MarkerBadge(text: Preset.planeText, color: Preset.planeColor, rotation: rotation)
    }

    static func routeStart() -> MarkerBadge {
        MarkerBadge(text: Preset.routeStartText, color: Preset.routeStartColor)
    }

    static func routeEnd() -> MarkerBadge {
        MarkerBadge(text: Preset.routeEndText, color: Preset.routeEndColor)
    }
}

private struct DirectionCone: Shape {
    var halfAngle: Double = 22

    func path(in rect: CGRect) -> Path {
        let center = CGPoint(x: rect.midX, y: rect.midY)
        let radius = min(rect.width, rect.height) / 2
        var path = Path()
        path.move(to: center)
        path.addArc(
            center: center,
            radius: radius,
            startAngle: .degrees(-90 - halfAngle),
            endAngle: .degrees(-90 + halfAngle),
            clockwise: false
        )
        path.closeSubpath()
        return path
    }
}

#Preview {
    VStack(spacing: 20) {
        MarkerBadge.baseStation()
        MarkerBadge.plane(rotation: 45)
    }
}
