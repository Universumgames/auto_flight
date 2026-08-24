import SwiftUI
import MapKit
import SharedLogic

/// Route visualization mirroring `RouteMap.vue`: planned route (blue), flown
/// history (red), base/plane markers with the plane rotated by heading.
struct RouteMapView: View {
    let plannedRoute: [Coordinate]
    let flightHistory: [Coordinate]
    let basePosition: Coordinate?
    let planePosition: Coordinate?
    let planeHeading: Double?

    @State private var cameraPosition: MapCameraPosition = .region(
        MKCoordinateRegion(
            center: CLLocationCoordinate2D(latitude: 51.316310347903176, longitude: 6.569530261539499),
            span: MKCoordinateSpan(latitudeDelta: 0.01, longitudeDelta: 0.01)
        )
    )
    @State private var didAutoFit = false

    var body: some View {
        Map(position: $cameraPosition) {
            if plannedRoute.count > 1 {
                MapPolyline(coordinates: plannedRoute.map(\.clCoordinate)).stroke(Color.blue, lineWidth: 4)
            }
            if flightHistory.count > 1 {
                MapPolyline(coordinates: flightHistory.map(\.clCoordinate)).stroke(Color.red, lineWidth: 4)
            }
            if let start = plannedRoute.first {
                Annotation("Start", coordinate: start.clCoordinate) { MarkerBadge.routeStart() }
            }
            if let end = plannedRoute.last, plannedRoute.count > 1 {
                Annotation("End", coordinate: end.clCoordinate) { MarkerBadge.routeEnd() }
            }
            if let base = basePosition {
                Annotation("Base Station", coordinate: base.clCoordinate) { MarkerBadge.baseStation() }
            }
            if let plane = planePosition {
                Annotation("Plane", coordinate: plane.clCoordinate) {
                    MarkerBadge.plane(rotation: planeHeading ?? 0)
                }
            }
        }
        .overlay(alignment: .topTrailing) {
            Button {
                fitToRoutes()
            } label: {
                Image(systemName: "scope")
                    .frame(width: 20, height: 20)
                    .padding(10)
                    .background(.thinMaterial)
                    .clipShape(Circle())
                    .shadow(radius: 2)
            }
            .padding()
        }
        .onChange(of: plannedRoute.count) { _, _ in fitToRoutes() }
        .onAppear { fitToRoutes() }
    }

    private func fitToRoutes() {
        let all = plannedRoute + flightHistory
        guard !all.isEmpty else { return }
        let lats = all.map(\.latitude)
        let lons = all.map(\.longitude)
        let center = CLLocationCoordinate2D(
            latitude: (lats.min()! + lats.max()!) / 2,
            longitude: (lons.min()! + lons.max()!) / 2
        )
        let span = MKCoordinateSpan(
            latitudeDelta: max((lats.max()! - lats.min()!) * 1.3, 0.01),
            longitudeDelta: max((lons.max()! - lons.min()!) * 1.3, 0.01)
        )
        withAnimation {
            cameraPosition = .region(MKCoordinateRegion(center: center, span: span))
        }
        didAutoFit = true
    }
}
