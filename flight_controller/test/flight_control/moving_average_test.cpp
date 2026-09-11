#include "MovingAverage.hpp"

#include <gtest/gtest.h>

TEST(MovingAverageTest, singleValueReturnsItself) {
    MovingAverage<3> avg;
    EXPECT_FLOAT_EQ(avg.push(5.0f), 5.0f);
}

TEST(MovingAverageTest, averagesWhileFilling) {
    MovingAverage<3> avg;
    avg.push(2.0f);
    EXPECT_FLOAT_EQ(avg.push(4.0f), 3.0f); // (2+4)/2
}

TEST(MovingAverageTest, averagesOverFullWindow) {
    MovingAverage<3> avg;
    avg.push(1.0f);
    avg.push(2.0f);
    EXPECT_FLOAT_EQ(avg.push(3.0f), 2.0f); // (1+2+3)/3
}

TEST(MovingAverageTest, forgetsSamplesOutsideWindow) {
    MovingAverage<3> avg;
    avg.push(10.0f);
    avg.push(10.0f);
    avg.push(10.0f);
    // window is now full of 10s; pushing three 0s should fully displace them
    avg.push(0.0f);
    avg.push(0.0f);
    EXPECT_FLOAT_EQ(avg.push(0.0f), 0.0f);
}

TEST(MovingAverageTest, windowSizeOneIsPassthrough) {
    MovingAverage<1> avg;
    EXPECT_FLOAT_EQ(avg.push(7.0f), 7.0f);
    EXPECT_FLOAT_EQ(avg.push(-3.0f), -3.0f);
}

TEST(HeadingMovingAverageTest, singleValueReturnsItself) {
    HeadingMovingAverage<3> avg;
    EXPECT_NEAR(avg.push(90.0f), 90.0f, 0.01f);
}

TEST(HeadingMovingAverageTest, averagesNormalValuesLikeArithmeticMean) {
    HeadingMovingAverage<3> avg;
    avg.push(10.0f);
    EXPECT_NEAR(avg.push(20.0f), 15.0f, 0.5f);
}

TEST(HeadingMovingAverageTest, wrapsCorrectlyAcrossZeroBoundary) {
    // Averaging 359 and 1 should give ~0, not 180 (a naive arithmetic mean
    // would be wrong here - this is the case the class exists to handle).
    HeadingMovingAverage<2> avg;
    avg.push(359.0f);
    const float result = avg.push(1.0f);
    const bool nearZero = result < 2.0f || result > 358.0f;
    EXPECT_TRUE(nearZero) << "got " << result;
}

TEST(HeadingMovingAverageTest, resultAlwaysInRange) {
    HeadingMovingAverage<4> avg;
    for (float heading = 0.0f; heading < 360.0f; heading += 13.0f) {
        const float result = avg.push(heading);
        EXPECT_GE(result, 0.0f);
        EXPECT_LT(result, 360.0f);
    }
}
