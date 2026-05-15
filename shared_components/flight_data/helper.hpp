#pragma once


inline bool isDevicePlane() {
#ifdef FLIGHT_DEVICE_TYPE_PLANE
    return true;
#else
    return false;
#endif
}

inline bool isDeviceBaseStation() {
#ifdef FLIGHT_DEVICE_TYPE_BASE_STATION
    return true;
#else
    return false;
#endif
}
