import SwiftUI
import MapKit
import CoreLocation
import SharedLogic

private let defaultCenter = Coordinate.Companion.shared.defaultLocation.clCoordinate

private class DeviceLocationManager: NSObject, ObservableObject, CLLocationManagerDelegate {
    private let manager = CLLocationManager()
    @Published private(set) var lastLocation: CLLocationCoordinate2D?

    override init() {
        super.init()
        manager.delegate = self
        manager.requestWhenInUseAuthorization()
        manager.startUpdatingLocation()
    }

    func locationManager(_ manager: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
        lastLocation = locations.last?.coordinate
    }
}

/// Tap-to-add-vertex polygon editor, mirroring `PolygonMap.vue` (which used
/// leaflet-draw). Native MapKit doesn't have a drag-to-edit polygon tool, so this
/// offers tap-to-append plus undo/clear instead.
struct PolygonMapView: View {
    @Binding var polygon: [Coordinate]
    let basePosition: Coordinate?
    let planePosition: Coordinate?
    let planeHeading: Double?

    @StateObject private var locationManager = DeviceLocationManager()
    @State private var cameraPosition: MapCameraPosition = .region(
        MKCoordinateRegion(center: defaultCenter, span: MKCoordinateSpan(latitudeDelta: 0.01, longitudeDelta: 0.01))
    )
    /// Set right before a programmatic camera move, so the `onMapCameraChange`
    /// callback it triggers isn't mistaken for user interaction.
    @State private var isRecentering = false
    /// Once the user has manually panned/zoomed, automatic recentering on
    /// base/device location becoming available stops.
    @State private var userMovedCamera = false

    var body: some View {
        MapReader { proxy in
            Map(position: $cameraPosition) {
                if polygon.count > 2 {
                    MapPolygon(coordinates: polygon.map(\.clCoordinate))
                        .foregroundStyle(Color.blue.opacity(0.2))
                        .stroke(Color.blue, lineWidth: 2)
                }
                ForEach(Array(polygon.enumerated()), id: \.offset) { _, coord in
                    Annotation("", coordinate: coord.clCoordinate) {
                        Circle().fill(.blue).frame(width: 10, height: 10)
                            .overlay(Circle().stroke(.white, lineWidth: 1.5))
                    }
                }
                if let base = basePosition {
                    Annotation(String(localized: "map.annotation.baseStation"), coordinate: base.clCoordinate) {
                        MarkerBadge.baseStation()
                    }
                }
                if let plane = planePosition {
                    Annotation(String(localized: "map.annotation.plane"), coordinate: plane.clCoordinate) {
                        MarkerBadge.plane(rotation: planeHeading)
                    }
                }
            }
            .gesture(
                SpatialTapGesture().onEnded { value in
                    if let coordinate = proxy.convert(value.location, from: .local) {
                        polygon.append(Coordinate(latitude: coordinate.latitude, longitude: coordinate.longitude))
                    }
                }
            )
            .overlay(alignment: .bottomTrailing) {
                VStack(spacing: 10) {
                    mapButton(systemImage: "scope") { recenter() }
                    if !polygon.isEmpty {
                        mapButton(systemImage: "arrow.uturn.backward") { polygon.removeLast() }
                        mapButton(systemImage: "trash") { polygon.removeAll() }
                    }
                }
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
        }
        .task { recenter() }
        .onChange(of: basePosition == nil) { wasNil, isNil in
            if wasNil, !isNil, !userMovedCamera {
                recenter()
            }
        }
        .onChange(of: locationManager.lastLocation == nil) { wasNil, isNil in
            if wasNil, !isNil, !userMovedCamera {
                recenter()
            }
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

    private func recenter() {
        let center = basePosition?.clCoordinate ?? locationManager.lastLocation ?? defaultCenter
        isRecentering = true
        userMovedCamera = false
        withAnimation {
            cameraPosition = .region(MKCoordinateRegion(center: center, span: MKCoordinateSpan(latitudeDelta: 0.01, longitudeDelta: 0.01)))
        }
    }
}

extension Coordinate {
    var clCoordinate: CLLocationCoordinate2D {
        CLLocationCoordinate2D(latitude: latitude, longitude: longitude)
    }
}

#Preview {
    PolygonMapView(
        polygon: .constant([]),
        basePosition: Coordinate.Companion.shared.defaultLocation,
        planePosition: Coordinate.Companion.shared.defaultLocation,
        planeHeading: 45
    )
}
