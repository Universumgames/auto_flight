
export interface SensorUpdatePacket {
  type?: "sensor"
  barometerPressureBase: number
  barometerPressurePlane: number
  calculatedAltitude: number
  headingPlane: number
}
