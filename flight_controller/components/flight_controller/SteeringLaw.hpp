#pragma once
#include <cstdint>

#include "types.hpp"

// Pure flight-control-law math, extracted out of FlightControllerClass so it
// can be exercised without any hardware/RTOS dependencies (native unit tests,
// simulation, etc). Behavior must stay identical to what previously lived
// inline in FlightControllerClass::steerToWaypoint.
namespace SteeringLaw {
    // Great-circle initial bearing in degrees [0, 360) from `from` to `to`.
    float computeBearing(Coordinate from, Coordinate to);

    // Signed difference between `bearingDeg` and `compassHeadingDeg`, normalized
    // to [-180, 180).
    float headingError(float bearingDeg, float compassHeadingDeg);

    // Maps a heading error to a rudder command: full deflection at +-90 deg of
    // error, scaled linearly in between, clamped to [rudderMin, rudderMax].
    // rudderMin is expected to be <= 0 and rudderMax >= 0 (independent travel
    // limits per direction, matching CONFIG_RUDDER_SERVO_MIN/MAX).
    int8_t computeRudder(float headingErrorDeg, int8_t rudderMin, int8_t rudderMax);
}

// Altitude-hold PI controller: drives thrust from altitude error and derives
// a target pitch angle. Owns its own integral state across calls.
class AltitudeHoldController {
public:
    struct Output {
        int8_t thrust;
        float targetPitchDeg;
    };

    Output update(float altitudeErrorM);

private:
    static constexpr float THRUST_BASE    = 50.0f;
    static constexpr float THRUST_P_GAIN  = 0.5f;
    static constexpr float THRUST_I_GAIN  = 0.3f;
    static constexpr float THRUST_I_LIMIT = 20.0f;

    // deg of target pitch per meter of altitude error
    static constexpr float ALTITUDE_TO_PITCH_GAIN = 0.3f;
    // clamp on target pitch degrees
    static constexpr float MAX_CLIMB_PITCH = 12.0f;

    float thrustIntegral = 0.0f;
};

// Attitude PI controller: keeps wings level (aileron) and drives toward a
// target pitch (elevator). Owns its own integral state across calls.
class AttitudeController {
public:
    struct Output {
        int8_t aileron;
        int8_t pitch;
    };

    // rollDeg: current roll angle. pitchErrorDeg: targetPitch - currentPitch.
    Output update(float rollDeg, float pitchErrorDeg);

private:
    static constexpr float AILERON_P_GAIN  = 1.5f;
    static constexpr float AILERON_I_GAIN  = 0.5f;
    static constexpr float AILERON_I_LIMIT = 30.0f;

    static constexpr float PITCH_P_GAIN  = 1.5f;
    static constexpr float PITCH_I_GAIN  = 0.5f;
    static constexpr float PITCH_I_LIMIT = 30.0f;

    float aileronIntegral = 0.0f;
    float pitchIntegral   = 0.0f;
};
