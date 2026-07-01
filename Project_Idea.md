# Initial Idea

## Basic Idea
An autonomous glider that flies over predefined areas, such as fields, and photographs them. The images can then be used, for example, to estimate crop yields, detect weeds, or identify other areas requiring action. In an emergency, the aircraft should be able to be controlled manually via a remote control.

## Communication, Configuration and Navigation
The aircraft communicates via LoRa with a base station, which provides a website via a hotspot that allows configuration of the flight area and other data.

An accelerometer, barometer, and magnetometer are used to determine the altitude and attitude of the aircraft. GPS is used to determine the correct position and thus ensure the automatic flying of the calculated waypoints.

## Possible Uses
By extending the system with a camera, aerial images can be captured and then used for the analysis of agricultural land.

Communication via LoRa allows for a long range but low bandwidth. It is therefore necessary to minimize the amount of data transmitted over LoRa. The images can therefore be stored on an SD card, with only the relevant data, such as GPS coordinates and status information, being transmitted via LoRa. As an alternative to storage on an SD card, which would have to be read out manually, the aircraft could transmit the images to the base station via WiFi upon landing. This would allow for faster and easier evaluation of the images, since they would be directly available on the base station's computer.

## Takeoff and Landing
For takeoff, the aircraft should be launched by hand and immediately begin automatic navigation. For landing, the aircraft should return to the starting point and either be landed manually, caught, or attempt to land gently on a meadow using a precise laser rangefinder.
