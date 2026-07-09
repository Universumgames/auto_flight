- [Weekly Learning Log](#weekly-learning-log)
  - [Week 1 (2026-04-24 - 2026-04-30)](#week-1-2026-04-24---2026-04-30)
  - [Week 2 (2026-05-01 - 2026-05-07)](#week-2-2026-05-01---2026-05-07)
  - [Week 3 (2026-05-08 - 2026-05-14)](#week-3-2026-05-08---2026-05-14)
  - [Week 4 (2026-05-15 - 2026-05-21)](#week-4-2026-05-15---2026-05-21)
  - [Week 5 (2026-05-22 - 2026-05-28)](#week-5-2026-05-22---2026-05-28)
  - [Week 6 (2026-05-29 - 2026-06-04)](#week-6-2026-05-29---2026-06-04)
  - [Week 7 (2026-06-05 - 2026-06-11)](#week-7-2026-06-05---2026-06-11)
  - [Week 8 (2026-06-12 - 2026-06-18)](#week-8-2026-06-12---2026-06-18)
  - [Week 9 (2026-06-19 - 2026-06-25)](#week-9-2026-06-19---2026-06-25)
  - [Week 10 (2026-06-26 - 2026-07-02)](#week-10-2026-06-26---2026-07-02)
  - [Week 11 (2026-07-03 - 2026-07-09)](#week-11-2026-07-03---2026-07-09)
  - [Week 12 (2026-07-10 - 2026-07-16)](#week-12-2026-07-10---2026-07-16)
- [Plans to do next](#plans-to-do-next)
- [Outlook](#outlook)


# Weekly Learning Log

## Week 1 (2026-04-24 - 2026-04-30)
- Project planning
- what components do I need

## Week 2 (2026-05-01 - 2026-05-07)
- researching legal issues with automatic drone
- project setup
- getting gps to work and decoding gps data

## Week 3 (2026-05-08 - 2026-05-14)
- researching and implementing simple path planning algorithm
- learning how to unit test and setting up unit tests for path planning
- re-learning how to fly and airplane

## Week 4 (2026-05-15 - 2026-05-21)
- switching to and learning to compile with the ESP-IDF and getting everything to compile (partly to fix issues with Platformio, using shared components and more easily run and debug unit tests on host machine)
- learning about component management and how to set up a project with shared components
- issues:
  - getting ESP-IDF to work with components
  - understanding components and dependency management

## Week 5 (2026-05-22 - 2026-05-28)
- setting up webserver for configuration and control
- getting WiFi working on base station
- learning about getting started with LoRa
- start with simple network stack over LoRa
- issues:
  - getting LoRa to work
  - understanding how to implement a network stack over LoRa

## Week 6 (2026-05-29 - 2026-06-04)
- getting LoRa connection working with acknowledgements and pings
- getting I2C working for barometer and gyroscope
- issues:
  - getting I2C working with multiple devices and libraries

## Week 7 (2026-06-05 - 2026-06-11)
- integrating components
- debugging and fixing LoRa communication and serialization issues
- issues:
  - fixing serialization, specifically arrays/vectors

## Week 8 (2026-06-12 - 2026-06-18)
- fixing serialization issues
- further integrating components and testing, fixing bugs
- issues:
  - trying to fix lora message fragmentation for packages larger than 255 bytes -> writing custom packet splitter
  - event handler queue deadlock

## Week 9 (2026-06-19 - 2026-06-25)
- writing orientation helpers for auto pilot
- get accellerometer working
- issues:
  - accelerometer not outputting correctly
  - calculate yaw and yaw drift
  
## Week 10 (2026-06-26 - 2026-07-02)
- soldering pcbs
- getting magnetometer working
- issues:
  - getting correct library working
  - non functional magnetometer
  - I2C stopped working on flight controller completely
  - Magnetometer answering to different addresses each time
- resolution ideas
  - getting HMC5883L magnetometer instead

## Week 11 (2026-07-03 - 2026-07-09)
- issues: 
  - I2C bus not working with constant disconnects
  - Magnetometer get stuck sending static (and wrong) data once the bus has one misread
- resolutions
  - switch Magnetometer read mode from continuous to single read mode
    interesting fact: when using continuous mode the magnetometer measures the magnetic field at a given frequency and stores the data in the output registers. When reading these registers via the I2C bus, instead of sending garbage (reading while the magnetometer is writing) or waiting or the last measurement, the magnetometer will lock up and destroys the I2C with garbage data. The solution, switching to single read mode, will wait for the magnetometer to finish measuring and then read the data.

## Week 12 (2026-07-10 - 2026-07-16)


# Plans to do next
1. Get compass working and calibrate it
2. get rudder (yaw) working so plane would steer to next waypoint

# Outlook
1. get roll working so the plane stays level
2. get pitch working to stay level (with manual motor speed)
3. get pitch working to steer to desired altitude (with manual motor speed)
4. get automatic motor speed working
5. automatic landing