#pragma once
#include <cstdint>

enum ControlCommand: uint8_t {
    /// Motor speed, percentage
    THRUST = 0x1,
    /// Up/Down,  signed int
    PITCH = 0x2,
    /// Left/Right Rudder, signed int
    YAW = 0x3,
    /// Left/Right Aileron difference in pitch, signed int
    AILERON_DIFF = 0x4,
};