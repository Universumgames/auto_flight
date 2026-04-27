#pragma once
#include <cstdint>
#include <lora.h>
#include <string>

class LoRa_Communication {

public:
    LoRa_Communication();
    ~LoRa_Communication();

    void begin();
    void end();

    void sendData(const uint8_t* data, int size);
    int receiveData(uint8_t* buffer, int size);
    bool hasReceivedData() const;
    int getLastPacketRSSI() const;
    float getLastPacketSNR() const;

private:
    uint8_t* encryptData(const uint8_t* originalData, int originalSize, int* encryptedSize);
    uint8_t* decryptData(const uint8_t* encryptedData, int encryptedSize, int* decryptedSize);

    uint8_t* key;// = new uint8_t[16]; // 128-bit key used for encryption

};
