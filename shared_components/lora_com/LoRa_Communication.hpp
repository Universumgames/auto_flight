#pragma once
#include <cstdint>
#include <memory>
#include <atomic>
#include <string>
#include <vector>
#include <algorithm>
#include <array>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "types.hpp"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#endif
#include <functional>

#include "modules/SX126x/SX1262.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include "LoRaPacket.hpp"


class LoRa_CommunicationClass {
private:
    LoRa_CommunicationClass() = default;

    enum class PacketType: uint8_t {
        HEADER,
        ACK,
        PING,
        DATA
    };

    static std::string toString(PacketType packetType);

    struct LoRa_Packet_Internal {
        PacketType type;
        /// hashed+truncated device identifier, stamped in sendRawPacket() before transmission
        uint8_t senderId[4];
        uint8_t messageId;
        uint8_t payloadLength;
        uint8_t fragmentId;
        uint8_t totalFragments;

        [[nodiscard]] std::string toString() const;

        static bool equals(const LoRa_Packet_Internal& a, const LoRa_Packet_Internal& b) {
            return a.type == b.type
                && std::equal(std::begin(a.senderId), std::end(a.senderId), std::begin(b.senderId))
                && a.messageId == b.messageId && a.payloadLength == b.payloadLength && a.fragmentId
                == b.fragmentId && a.totalFragments == b.totalFragments;
        }

        constexpr bool operator==(const LoRa_Packet_Internal& b) const {
            return equals(*this, b);
        }
    };

    struct ReceivedPacket {
        struct SimpleHeader {
            PacketType type;
            uint8_t senderId[4];
            uint8_t messageId;
            uint8_t totalFragments;
            size_t payloadLength;
        };
        SimpleHeader header;
        std::unique_ptr<uint8_t[]> payload;
    };

    struct PacketFragment {
        uint8_t messageId;
        uint8_t fragmentId;
        uint8_t payloadLength;
        std::unique_ptr<uint8_t[]> payload;

        bool operator==(const PacketFragment& b) const {
            return messageId == b.messageId && fragmentId == b.fragmentId;
        }
    };

    struct ReceivedFragmentsCache {
        LoRa_Packet_Internal header;
        std::vector<PacketFragment> fragments;
    };

    static constexpr size_t LORA_MAX_PACKET_SIZE = 255;
    static constexpr size_t LORA_MAX_DATA_LENGTH = LORA_MAX_PACKET_SIZE - sizeof(LoRa_Packet_Internal);
    const char* TAG_LORA = "LoRa_Communication";

    static constexpr TickType_t LORA_RX_POLL_DELAY = pdMS_TO_TICKS(10);
    static constexpr TickType_t LORA_SEND_DELAY = pdMS_TO_TICKS(10);
    /// receive task re-checks the IRQ flags at least this often, in case a DIO1 edge was missed
    static constexpr TickType_t LORA_RX_IRQ_FALLBACK_POLL = pdMS_TO_TICKS(100);
    /// extra time on top of the airtime-based ACK timeout (processing, task scheduling)
    static constexpr uint32_t LORA_ACK_MARGIN_MS = 100;
    static constexpr TickType_t LORA_PING_CHECK_INTERVAL = pdMS_TO_TICKS(1000); // Check every 1 second
    static constexpr int LORA_MAX_SEND_RETRIES = 3;

    /// number of recently received (sender, message, fragment) ids remembered to detect retransmissions
    static constexpr size_t RECENT_RX_HISTORY = 32;

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
     * @return true if the data was sent successfully, false otherwise
     */
    bool sendData(const uint8_t* data, size_t size);

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

    /**
     * Registers a callback function to be invoked when a new LoRa packet is received
     * @param callback The callback function to register; it takes a const reference to a LoRaPacket, data pointer is valid only during the callback execution
     */
    void registerReceivePacketCallback(std::function<void(const LoRaPacket&)> callback);

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

    static void dio0_isr_handler(void* args);

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

    static void callbackWorkerEntry(void* param);

    [[noreturn]] void callbackWorkerLoop();

    static void joinFragmentsEntry(void* param);

    [[noreturn]] void joinFragmentsLoop();

    /**
     * Sends a raw LoRa packet with optional acknowledgement requirement
     * @param packet The packet header to send
     * @param data Pointer to the payload data (can be nullptr if no payload)
     * @param requireAck If true, waits for acknowledgement; defaults to true
     * @return true on success, false otherwise
     */
    bool sendRawPacket(LoRa_Packet_Internal packet, const uint8_t* data, bool requireAck = true);

    /**
     * Updates the timestamp of the last successful transmission
     */
    void updateLastSendTime();

    /**
     * Updates the current connection state and notifies listeners if state changed.
     * Plane firmware only ever talks to the single base station, so no id is needed;
     * on the base station this is a no-op, since a send's success/failure carries no
     * addressee (see the id-taking overload, used from the receive path instead).
     * @param state The new connection state
     */
    void updateConnectionState(ConnectionState state);

#ifdef FLIGHT_DEVICE_TYPE_BASE_STATION
    /**
     * Updates the connection state of a specific plane, identified by its real device id as
     * parsed from a received packet header (LoRa_Packet_Internal::senderId).
     * @param planeId The plane's device id
     * @param state The new connection state
     */
    void updateConnectionState(uint32_t planeId, ConnectionState state);
#endif

    /**
     * Sends an ACK for a received packet. The ACK echoes messageId and fragmentId and carries the
     * original sender's id as payload, so only that device accepts it.
     * Must only be called from the receive task.
     * @param received The header of the packet being acknowledged
     */
    void sendAckPacketInternal(const LoRa_Packet_Internal& received);

    /**
     * Processes a received ACK or PING packet
     * @param receivedHeader The received packet header
     * @param data Payload following the header
     * @param size Size of the payload
     */
    void processPacket(const LoRa_Packet_Internal& receivedHeader, const uint8_t* data, int size);

    void processDataPacket(const LoRa_Packet_Internal& header, const uint8_t* data, int size);

    /**
     * Transmits a buffer and puts the radio back into receive mode.
     * TX and RX share the radio's FIFO and transmit() aborts an ongoing reception, so unless
     * deferToReceiver is false this waits until no packet is being received or waiting to be read (bounded),
     * the receive task has sent its ACK for the last received packet, and the peer has finished
     * sending a fragmented message to us.
     * @return true if the radio reported a successful transmission
     */
    bool transmitWhenChannelClear(const uint8_t* buf, size_t len, bool deferToReceiver);

    /**
     * Remembers a received data fragment and reports whether it was already received before
     * (i.e. the sender retransmitted because our ACK got lost). Only called from the receive task.
     */
    bool isDuplicateFragment(const LoRa_Packet_Internal& header);

    std::vector<LoRa_Packet_Internal> splitData(size_t size);

    ReceivedPacket joinData(ReceivedFragmentsCache& fragmentCache) const;

    uint8_t* key = nullptr; // 128-bit key used for encryption
    TaskHandle_t receiveTaskHandle = nullptr;
    TaskHandle_t pingTaskHandle = nullptr;
    TaskHandle_t callbackWorkerHandle = nullptr;
    TaskHandle_t joinFragmentsHandle = nullptr;
    SemaphoreHandle_t radioMutex = nullptr;
    SemaphoreHandle_t receivedPacketsMutex = nullptr;
    SemaphoreHandle_t receivedFragmentsMutex = nullptr;
    SemaphoreHandle_t ackMutex = nullptr;
    SemaphoreHandle_t lastSendTimeMutex = nullptr;
    std::vector<ReceivedPacket> receivedPackets;
    std::unordered_map<uint8_t, ReceivedFragmentsCache> receivedFragments; // messageId -> received fragment data
    struct OutstandingAck {
        uint8_t messageId;
        uint8_t fragmentId;

        bool operator==(const OutstandingAck& b) const = default;
    };
    std::vector<OutstandingAck> outstandingAcks;
    /// serializes ACK-requiring sends of this device: while waiting for an ACK no other local packet may be
    /// transmitted, the radio is half-duplex and would miss the ACK
    SemaphoreHandle_t sendMutex = nullptr;
    /// set by the receive task from reading a packet until its ACK is sent, so other tasks can't take the radio first
    std::atomic<bool> ackPending{false};
    /// while a peer is sending us a fragmented message, other senders keep the channel free until this tick
    std::atomic<TickType_t> channelReservedUntil{0};
    TickType_t lastSendTime = 0;
    std::atomic<uint8_t> nextMessageId{1};
    /// how long to wait for an ACK, derived from the configured modulation in begin()
    uint32_t ackTimeoutMs = 1000;
    /// upper bound for waiting on an incoming packet before transmitting anyway; also the retry jitter range,
    /// so two devices that collided don't retransmit into each other again
    uint32_t maxPacketAirtimeMs = 1000;

    struct RecentRx {
        bool valid;
        uint8_t senderId[4];
        uint8_t messageId;
        uint8_t fragmentId;

        bool operator==(const RecentRx& b) const = default;
    };
    std::array<RecentRx, RECENT_RX_HISTORY> recentRx{};
    size_t recentRxNext = 0;
    // Whether a DIO0 ISR has been installed for the radio (used by the receive task)
    bool dio0IsrInstalled = false;

    bool sending = false;

    SX1262* loraRadio = nullptr;

    std::vector<std::function<void(const LoRaPacket&)>> receivePacketCallbacks;
};

extern LoRa_CommunicationClass& LoRa_Communication;
