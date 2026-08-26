#include "SteeringLaw.hpp"

#include <gtest/gtest.h>

TEST(SteeringLawBearingTest, dueNorth) {
    const Coordinate from{0, 0};
    const Coordinate to{0, 1}; // longitude unchanged, latitude increases -> north
    EXPECT_NEAR(SteeringLaw::computeBearing(from, to), 0.0f, 0.5f);
}

TEST(SteeringLawBearingTest, dueEast) {
    const Coordinate from{0, 0};
    const Coordinate to{1, 0}; // longitude increases -> east
    EXPECT_NEAR(SteeringLaw::computeBearing(from, to), 90.0f, 0.5f);
}

TEST(SteeringLawBearingTest, dueSouth) {
    const Coordinate from{0, 0};
    const Coordinate to{0, -1};
    EXPECT_NEAR(SteeringLaw::computeBearing(from, to), 180.0f, 0.5f);
}

TEST(SteeringLawBearingTest, dueWest) {
    const Coordinate from{0, 0};
    const Coordinate to{-1, 0};
    EXPECT_NEAR(SteeringLaw::computeBearing(from, to), 270.0f, 0.5f);
}

TEST(SteeringLawBearingTest, alwaysInRange) {
    const Coordinate from{12, -5};
    const Coordinate to{-40, 30};
    const float bearing = SteeringLaw::computeBearing(from, to);
    EXPECT_GE(bearing, 0.0f);
    EXPECT_LT(bearing, 360.0f);
}

TEST(SteeringLawHeadingErrorTest, zeroWhenAligned) {
    EXPECT_FLOAT_EQ(SteeringLaw::headingError(90.0f, 90.0f), 0.0f);
}

TEST(SteeringLawHeadingErrorTest, positiveWhenBearingAhead) {
    EXPECT_FLOAT_EQ(SteeringLaw::headingError(100.0f, 90.0f), 10.0f);
}

TEST(SteeringLawHeadingErrorTest, negativeWhenBearingBehind) {
    EXPECT_FLOAT_EQ(SteeringLaw::headingError(80.0f, 90.0f), -10.0f);
}

TEST(SteeringLawHeadingErrorTest, wrapsAcrossZero) {
    // heading 350, bearing 10 -> shortest turn is +20, not -340
    EXPECT_FLOAT_EQ(SteeringLaw::headingError(10.0f, 350.0f), 20.0f);
}

TEST(SteeringLawHeadingErrorTest, wrapsAcrossZeroOtherDirection) {
    // heading 10, bearing 350 -> shortest turn is -20, not +340
    EXPECT_FLOAT_EQ(SteeringLaw::headingError(350.0f, 10.0f), -20.0f);
}

TEST(SteeringLawHeadingErrorTest, resultAlwaysInRange) {
    for (float bearing = 0.0f; bearing < 360.0f; bearing += 17.0f) {
        for (float heading = 0.0f; heading < 360.0f; heading += 23.0f) {
            const float error = SteeringLaw::headingError(bearing, heading);
            EXPECT_GE(error, -180.0f);
            EXPECT_LT(error, 180.0f);
        }
    }
}

TEST(SteeringLawRudderTest, zeroErrorGivesZeroRudder) {
    EXPECT_EQ(SteeringLaw::computeRudder(0.0f, -100, 100), 0);
}

TEST(SteeringLawRudderTest, clampsAtPositiveFullDeflection) {
    // >= 90 deg error clamps to full deflection towards rudderMax
    EXPECT_EQ(SteeringLaw::computeRudder(90.0f, -100, 100), 100);
    EXPECT_EQ(SteeringLaw::computeRudder(180.0f, -100, 100), 100);
}

TEST(SteeringLawRudderTest, clampsAtNegativeFullDeflection) {
    // <= -90 deg error clamps to full deflection towards -rudderMin
    EXPECT_EQ(SteeringLaw::computeRudder(-90.0f, -100, 100), -100);
    EXPECT_EQ(SteeringLaw::computeRudder(-180.0f, -100, 100), -100);
}

TEST(SteeringLawRudderTest, respectsAsymmetricServoLimits) {
    EXPECT_EQ(SteeringLaw::computeRudder(90.0f, -60, 80), 80);
    EXPECT_EQ(SteeringLaw::computeRudder(-90.0f, -60, 80), -60);
}

TEST(SteeringLawRudderTest, scalesLinearlyInBetween) {
    // 45 deg error is half of the 90 deg full-deflection range
    EXPECT_EQ(SteeringLaw::computeRudder(45.0f, -100, 100), 50);
}

class AltitudeHoldControllerTest : public ::testing::Test {
protected:
    AltitudeHoldController controller;
};

TEST_F(AltitudeHoldControllerTest, zeroErrorGivesBaseThrustAndLevelPitch) {
    const auto out = controller.update(0.0f);
    EXPECT_EQ(out.thrust, 50);
    EXPECT_FLOAT_EQ(out.targetPitchDeg, 0.0f);
}

TEST_F(AltitudeHoldControllerTest, positiveErrorClimbsAndIncreasesThrust) {
    const auto out = controller.update(20.0f);
    EXPECT_GT(out.thrust, 50);
    EXPECT_GT(out.targetPitchDeg, 0.0f);
}

TEST_F(AltitudeHoldControllerTest, negativeErrorDescendsAndDecreasesThrust) {
    const auto out = controller.update(-20.0f);
    EXPECT_LT(out.thrust, 50);
    EXPECT_LT(out.targetPitchDeg, 0.0f);
}

TEST_F(AltitudeHoldControllerTest, targetPitchClampsAtMaxClimb) {
    const auto out = controller.update(1000.0f);
    EXPECT_LE(out.targetPitchDeg, 12.0f);
}

TEST_F(AltitudeHoldControllerTest, targetPitchClampsAtMaxDescent) {
    const auto out = controller.update(-1000.0f);
    EXPECT_GE(out.targetPitchDeg, -12.0f);
}

TEST_F(AltitudeHoldControllerTest, thrustNeverExceedsBounds) {
    for (int i = 0; i < 200; i++) {
        const auto out = controller.update(1000.0f);
        EXPECT_GE(out.thrust, 0);
        EXPECT_LE(out.thrust, 100);
    }
}

TEST_F(AltitudeHoldControllerTest, integralAccumulatesAcrossCalls) {
    // Error large enough that the growing integral term crosses an int8_t
    // boundary between calls, so truncation doesn't mask the accumulation.
    const auto first = controller.update(50.0f);
    const auto second = controller.update(50.0f);
    EXPECT_GT(second.thrust, first.thrust);
}

class AttitudeControllerTest : public ::testing::Test {
protected:
    AttitudeController controller;
};

TEST_F(AttitudeControllerTest, levelAndOnTargetGivesZeroOutputs) {
    const auto out = controller.update(0.0f, 0.0f);
    EXPECT_EQ(out.aileron, 0);
    EXPECT_EQ(out.pitch, 0);
}

TEST_F(AttitudeControllerTest, positiveRollCountersteersAileron) {
    const auto out = controller.update(20.0f, 0.0f);
    EXPECT_GT(out.aileron, 0);
}

TEST_F(AttitudeControllerTest, positivePitchErrorDrivesPitchOutput) {
    const auto out = controller.update(0.0f, 20.0f);
    EXPECT_GT(out.pitch, 0);
}

TEST_F(AttitudeControllerTest, outputsNeverExceedBounds) {
    for (int i = 0; i < 200; i++) {
        const auto out = controller.update(1000.0f, 1000.0f);
        EXPECT_GE(out.aileron, -100);
        EXPECT_LE(out.aileron, 100);
        EXPECT_GE(out.pitch, -100);
        EXPECT_LE(out.pitch, 100);
    }
}
