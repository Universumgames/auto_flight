#pragma once
#include <cstdint>
#include <memory>
#include <atomic>
#include <string>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "types.hpp"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#endif
#include "modules/SX126x/SX1262.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

class LoRa_CommunicationClass {
private:
    LoRa_CommunicationClass() = default;

    enum class PacketType: uint8_t {
        HEADER,
        ACK,
        PING
    };

    static std::string toString(PacketType packetType);

    struct LoRa_Packet {
        PacketType type;
        uint8_t messageId;
        uint8_t payloadLength;

        [[nodiscard]] std::string toString() const;
    };

    struct ReceivedPacket {
        LoRa_Packet header;
        std::unique_ptr<uint8_t[]> payload;
    };
public:
    ~LoRa_CommunicationClass() = delete;

    /**
     * Returns a reference to the singleton instance of LoRa_CommunicationClass
     * @return Reference to the singleton instance
     */
    static LoRa_CommunicationClass& getInstance();

    /**
     * Returns a pointer to the singleton instance of LoRa_CommunicationClass
     * @return Pointer to the singleton instance
     */
    static LoRa_CommunicationClass* getInstancePtr();

    /**
     * Initializes the LoRa communication module and starts background tasks
     */
    void begin();

    /**
     * Sends data packet via LoRa radio with encryption and optional acknowledgement
     * @param data Pointer to the data to be sent
     * @param size Number of bytes to send
     */
    void sendData(const uint8_t* data, uint8_t size);

    /**
     * Receives data from the received packets queue into the provided buffer
     * @param buffer Pointer to the destination buffer
     * @param size Maximum number of bytes to read into the buffer
     * @return Number of bytes actually received, or -1 on error
     */
    int receiveData(uint8_t* buffer, int size);

    /**
     * Checks if there is received data waiting in the queue
     * @return true if data is available, false otherwise
     */
    [[nodiscard]] bool hasReceivedData() const;

    /**
     * Gets the RSSI (Received Signal Strength Indicator) of the last received packet
     * @return RSSI value in dBm
     */
    [[nodiscard]] float getLastPacketRSSI() const;

    /**
     * Gets the SNR (Signal-to-Noise Ratio) of the last received packet
     * @return SNR value in dB
     */
    [[nodiscard]] float getLastPacketSNR() const;

private:
    /**
     * Encrypts data using AES-128-GCM algorithm
     * @param originalData Pointer to the plaintext data to encrypt
     * @param originalSize Size of the plaintext data in bytes
     * @return Vector containing encrypted packet (nonce|ciphertext|tag), or empty vector on failure
     */
    std::vector<uint8_t> encryptData(const uint8_t* originalData, int originalSize);

    /**
     * Decrypts data encrypted with encryptData()
     * @param encryptedData Pointer to the encrypted packet (nonce|ciphertext|tag)
     * @param encryptedSize Size of the encrypted data in bytes
     * @return Vector containing decrypted plaintext, or empty vector on failure
     */
    std::vector<uint8_t> decryptData(const uint8_t* encryptedData, int encryptedSize);

    /**
     * FreeRTOS task entry point for the receive task (static wrapper)
     * @param param Pointer to the LoRa_CommunicationClass instance
     */
    static void receiveTaskEntry(void* param);

    /**
     * Main receive task loop that continuously monitors for incoming packets
     * Blocks until data is available or timeout occurs
     */
    [[noreturn]] void receiveTaskLoop();

    /**
     * FreeRTOS task entry point for the ping task (static wrapper)
     * @param param Pointer to the LoRa_CommunicationClass instance
     */
    static void pingTaskEntry(void* param);

    /**
     * Main ping task loop that periodically sends ping packets to maintain connection
     */
    [[noreturn]] void pingTaskLoop();

    /**
     * Sends a raw LoRa packet with optional acknowledgement requirement
     * @param packet The packet header to send
     * @param data Pointer to the payload data (can be nullptr if no payload)
     * @param requireAck If true, waits for acknowledgement; defaults to true
     * @return true on success, false otherwise
     */
    bool sendRawPacket(const LoRa_Packet& packet, const uint8_t* data, bool requireAck = true);

    /**
     * Updates the timestamp of the last successful transmission
     */
    void updateLastSendTime();

    /**
     * Updates the current connection state and notifies listeners if state changed
     * @param state The new connection state
     */
    void updateConnectionState(ConnectionState state);

    /**
     * Sends an ACK packet for a received message
     * @param messageId The ID of the message being acknowledged
     */
    void sendAckPacketInternal(uint8_t messageId);

    /**
     * Waits for a payload packet with the expected size
     * @param dataBuffer Buffer to store received payload
     * @param expectedSize Expected size of the payload
     * @return Number of bytes received, or -1 on timeout
     */
    int waitForPayload(uint8_t* dataBuffer, uint8_t expectedSize);

    /**
     * Processes a received LoRa packet header
     * @param receivedHeader The received packet header
     */
    void processPacket(const LoRa_Packet& receivedHeader);

    uint8_t* key = nullptr; // 128-bit key used for encryption
    TaskHandle_t receiveTaskHandle = nullptr;
    TaskHandle_t pingTaskHandle = nullptr;
    SemaphoreHandle_t radioMutex = nullptr;
    SemaphoreHandle_t receivedPacketsMutex = nullptr;
    SemaphoreHandle_t ackMutex = nullptr;
    SemaphoreHandle_t lastSendTimeMutex = nullptr;
    std::vector<ReceivedPacket> receivedPackets;
    std::vector<uint8_t> receivedAcks;
    TickType_t lastSendTime = 0;
    std::atomic<uint8_t> nextMessageId{1};
    // Whether a DIO0 ISR has been installed for the radio (used by the receive task)
    bool dio0IsrInstalled = false;

    SX1262* loraRadio = nullptr;

};

extern LoRa_CommunicationClass& LoRa_Communication;

extern const char* TAG_LORA;
