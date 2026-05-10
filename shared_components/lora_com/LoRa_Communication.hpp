#pragma once
#include <cstdint>
#include <string>

#include "route.hpp"

class LoRa_Communication {

public:
    LoRa_Communication();
    ~LoRa_Communication();

    void begin();
    void end();

    void sendData(const uint8_t* data, int size);
    int receiveData(uint8_t* buffer, int size);
    [[nodiscard]] bool hasReceivedData() const;
    [[nodiscard]] int getLastPacketRSSI() const;
    [[nodiscard]] float getLastPacketSNR() const;

    LoRa_Communication& operator<<(const Route& route);
    LoRa_Communication& operator>>(Route& route);

    LoRa_Communication& operator<<(const Coordinate& coordinates);
    LoRa_Communication& operator>>(Coordinate& coordinates);

private:
    uint8_t* encryptData(const uint8_t* originalData, int originalSize, int* encryptedSize);
    uint8_t* decryptData(const uint8_t* encryptedData, int encryptedSize, int* decryptedSize);

    uint8_t* key;// = new uint8_t[16]; // 128-bit key used for encryption

};
