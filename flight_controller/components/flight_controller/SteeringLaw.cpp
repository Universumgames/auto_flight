#include "SteeringLaw.hpp"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float PI_F = 3.14159265358979323846f;
}

float SteeringLaw::computeBearing(const Coordinate from, const Coordinate to) {
    const float dLon = (to.longitude - from.longitude) * PI_F / 180.0f;
    const float lat1 = from.latitude * PI_F / 180.0f;
    const float lat2 = to.latitude * PI_F / 180.0f;
    const float y = sinf(dLon) * cosf(lat2);
    const float x = cosf(lat1) * sinf(lat2) - sinf(lat1) * cosf(lat2) * cosf(dLon);
    float bearing = atan2f(y, x) * 180.0f / PI_F;
    return fmodf(bearing + 360.0f, 360.0f);
}

float SteeringLaw::headingError(const float bearingDeg, const float compassHeadingDeg) {
    float error = bearingDeg - compassHeadingDeg;
    if (error >= 180.0f) error -= 360.0f;
    if (error < -180.0f) error += 360.0f;
    return error;
}

int8_t SteeringLaw::computeRudder(const float headingErrorDeg, const int8_t rudderMin, const int8_t rudderMax) {
    const float normalized = std::max(-1.0f, std::min(1.0f, headingErrorDeg / 90.0f));
    const float rudderF = normalized >= 0.0f
                              ? normalized * static_cast<float>(rudderMax)
                              : normalized * static_cast<float>(-rudderMin);
    return static_cast<int8_t>(rudderF);
}

AltitudeHoldController::Output AltitudeHoldController::update(const float altitudeErrorM) {
    thrustIntegral = std::max(-THRUST_I_LIMIT, std::min(THRUST_I_LIMIT,
                                                        thrustIntegral + altitudeErrorM * 0.05f));
    const float thrustF = THRUST_BASE + altitudeErrorM * THRUST_P_GAIN + thrustIntegral * THRUST_I_GAIN;
    const auto thrust = static_cast<int8_t>(std::max(0.0f, std::min(100.0f, thrustF)));

    const float targetPitch = std::max(-MAX_CLIMB_PITCH, std::min(MAX_CLIMB_PITCH,
                                                                  altitudeErrorM * ALTITUDE_TO_PITCH_GAIN));

    return {thrust, targetPitch};
}

AttitudeController::Output AttitudeController::update(const float rollDeg, const float pitchErrorDeg) {
    aileronIntegral = std::max(-AILERON_I_LIMIT, std::min(AILERON_I_LIMIT,
                                                          aileronIntegral + rollDeg * 0.05f));
    const float aileronF = rollDeg * AILERON_P_GAIN + aileronIntegral * AILERON_I_GAIN;
    const auto aileron = static_cast<int8_t>(std::max(-100.0f, std::min(100.0f, aileronF)));

    pitchIntegral = std::max(-PITCH_I_LIMIT, std::min(PITCH_I_LIMIT,
                                                      pitchIntegral + pitchErrorDeg * 0.05f));
    const float pitchF = pitchErrorDeg * PITCH_P_GAIN + pitchIntegral * PITCH_I_GAIN;
    const auto pitch = static_cast<int8_t>(std::max(-100.0f, std::min(100.0f, pitchF)));

    return {aileron, pitch};
}
