import SwiftUI
import MapKit
import SharedLogic

/// Route visualization mirroring `RouteMap.vue`: planned route (blue), flown
/// history (red), the submitted area boundary (dashed gray), base/plane
/// markers with the plane rotated by heading.
struct RouteMapView: View {
    let plannedRoute: [Coordinate]
    let flightHistory: [Coordinate]
    /// The area polygon submitted in `AreaView`, used to fit the camera while
    /// waiting for a planned route (both `plannedRoute` and `flightHistory` empty).
    let areaPolygon: [Coordinate]
    let basePosition: Coordinate?
    let planePosition: Coordinate?
    let planeHeading: Double?

    @State private var cameraPosition: MapCameraPosition = .region(
        MKCoordinateRegion(
            center: Coordinate.Companion.shared.defaultLocation.clCoordinate,
            span: MKCoordinateSpan(latitudeDelta: 0.01, longitudeDelta: 0.01)
        )
    )
    /// Set right before a programmatic camera move, so the `onMapCameraChange`
    /// callback it triggers isn't mistaken for user interaction.
    @State private var isRecentering = false
    /// Once the user has manually panned/zoomed, automatic fitting on new data
    /// (route arriving, base/plane position becoming available) stops.
    @State private var userMovedCamera = false

    var body: some View {
        Map(position: $cameraPosition) {
            if areaPolygon.count > 2 {
                MapPolygon(coordinates: areaPolygon.map(\.clCoordinate))
                    .foregroundStyle(Color.gray.opacity(0.15))
                    .stroke(Color.gray, style: StrokeStyle(lineWidth: 2, dash: [6, 4]))
            }
            if plannedRoute.count > 1 {
                MapPolyline(coordinates: plannedRoute.map(\.clCoordinate)).stroke(Color.blue, lineWidth: 4)
            }
            if flightHistory.count > 1 {
                MapPolyline(coordinates: flightHistory.map(\.clCoordinate)).stroke(Color.red, lineWidth: 4)
            }
            if let start = plannedRoute.first {
                Annotation(String(localized: "map.annotation.routeStart"), coordinate: start.clCoordinate) { MarkerBadge.routeStart() }
            }
            if let end = plannedRoute.last, plannedRoute.count > 1 {
                Annotation(String(localized: "map.annotation.routeEnd"), coordinate: end.clCoordinate) { MarkerBadge.routeEnd() }
            }
            if let base = basePosition {
                Annotation(String(localized: "map.annotation.baseStation"), coordinate: base.clCoordinate) { MarkerBadge.baseStation() }
            }
            if let plane = planePosition {
                Annotation(String(localized: "map.annotation.plane"), coordinate: plane.clCoordinate) {
                    MarkerBadge.plane(rotation: planeHeading ?? 0)
                }
            }
        }
        .overlay(alignment: .bottomTrailing) {
            mapButton(systemImage: "scope") { fitToRoutes() }
                .padding([.trailing, .bottom], 16)
        }
        .mapControls {
            MapScaleView()
            MapCompass()
        }
        .mapControlVisibility(.visible)
        .onMapCameraChange(frequency: .onEnd) { _ in
            if isRecentering {
                isRecentering = false
            } else {
                userMovedCamera = true
            }
        }
        .onAppear { fitToRoutes() }
        .onChange(of: plannedRoute.count) { _, _ in
            if !userMovedCamera { fitToRoutes() }
        }
        .onChange(of: basePosition == nil) { wasNil, isNil in
            if wasNil, !isNil, !userMovedCamera { fitToRoutes() }
        }
        .onChange(of: planePosition == nil) { wasNil, isNil in
            if wasNil, !isNil, !userMovedCamera { fitToRoutes() }
        }
    }

    private func mapButton(systemImage: String, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Image(systemName: systemImage)
                .frame(width: 20, height: 20)
                .padding(10)
                .background(.thinMaterial)
                .clipShape(Circle())
                .shadow(radius: 2)
        }
    }

    private func fitToRoutes() {
        let routeCoordinates = plannedRoute + flightHistory
        // No route yet: fit around the submitted area and whatever positions
        // are already known, instead of leaving the camera at its default.
        let coordinates = routeCoordinates.isEmpty
            ? areaPolygon + [basePosition, planePosition].compactMap { $0 }
            : routeCoordinates
        guard !coordinates.isEmpty else { return }
        let lats = coordinates.map(\.latitude)
        let lons = coordinates.map(\.longitude)
        let center = CLLocationCoordinate2D(
            latitude: (lats.min()! + lats.max()!) / 2,
            longitude: (lons.min()! + lons.max()!) / 2
        )
        let span = MKCoordinateSpan(
            latitudeDelta: max((lats.max()! - lats.min()!) * 1.3, 0.01),
            longitudeDelta: max((lons.max()! - lons.min()!) * 1.3, 0.01)
        )
        isRecentering = true
        userMovedCamera = false
        withAnimation {
            cameraPosition = .region(MKCoordinateRegion(center: center, span: span))
        }
    }
}

#Preview {
    RouteMapView(
        plannedRoute: [],
        flightHistory: [],
        areaPolygon: [],
        basePosition: Coordinate.Companion.shared.defaultLocation,
        planePosition: nil,
        planeHeading: nil,
    )
    .environment(AppState.shared)
    .environment(ConnectionManager())
}
