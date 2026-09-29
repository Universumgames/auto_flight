#include "LoRa_Communication.hpp"

#include "esp_log.h"
#include <algorithm>
#include <cstring>

#include "DeviceId.hpp"
#include "driver/gpio.h"

#include "FlightStorage.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "EspHal.h"
#include "mutex_helper.hpp"

static LoRa_CommunicationClass* lo_ra_communication = nullptr;

LoRa_CommunicationClass& LoRa_Communication = LoRa_CommunicationClass::getInstance();

LoRa_CommunicationClass* LoRa_CommunicationClass::getInstancePtr() {
    if (lo_ra_communication == nullptr) {
        lo_ra_communication = new LoRa_CommunicationClass();
    }
    return lo_ra_communication;
}

void IRAM_ATTR LoRa_CommunicationClass::dio0_isr_handler(void* args) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(static_cast<TaskHandle_t>(args), &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

LoRa_CommunicationClass& LoRa_CommunicationClass::getInstance() {
    return *getInstancePtr();
}

void LoRa_CommunicationClass::begin() {
    ESP_LOGI(TAG_LORA, "LoRa communication initializing");

    ESP_LOGI(TAG_LORA, "creating semaphores");

    if (radioMutex == nullptr) {
        radioMutex = xSemaphoreCreateMutex();
    }
    if (receivedPacketsMutex == nullptr) {
        receivedPacketsMutex = xSemaphoreCreateMutex();
    }
    if (receivedFragmentsMutex == nullptr) {
        receivedFragmentsMutex = xSemaphoreCreateMutex();
    }
    if (ackMutex == nullptr) {
        ackMutex = xSemaphoreCreateMutex();
    }
    if (lastSendTimeMutex == nullptr) {
        lastSendTimeMutex = xSemaphoreCreateMutex();
    }
    if (radioMutex == nullptr || receivedPacketsMutex == nullptr || lastSendTimeMutex == nullptr || ackMutex ==
        nullptr) {
        ESP_LOGE(TAG_LORA, "Failed to create LoRa mutexes");
        return;
    }

    outstandingAcks.reserve(20);
    sentPackets.reserve(SENT_PACKET_HISTORY_MAX + 1);

    ESP_LOGI(TAG_LORA, "initializing LoRa radio");

    // Construct RadioLib HAL/Module and SX1262 instance
    auto* hal = new EspHal(CONFIG_LORA_SCK_GPIO, CONFIG_LORA_MISO_GPIO, CONFIG_LORA_MOSI_GPIO);
    auto module = new Module(hal, CONFIG_LORA_CS_GPIO, CONFIG_LORA_DIO0_PIN, CONFIG_LORA_RST_GPIO,
                             CONFIG_LORA_BUSY_GPIO);
    this->loraRadio = new SX1262(module);

    // create SX1262 instance and initialize with RadioLib
    int16_t initRes = this->loraRadio->begin(
        static_cast<float>(CONFIG_LORA_FREQUENCY) / 1e6f, // freq in MHz
        static_cast<float>(CONFIG_LORA_BANDWIDTH) / 1000.0f, // bw in kHz
        static_cast<uint8_t>(CONFIG_LORA_SPREADING_FACTOR), // SF
        static_cast<uint8_t>(CONFIG_LORA_CODING_RATE_DENOMINATOR), // coding rate denom
        static_cast<uint8_t>(CONFIG_LORA_SYNC_WORD), // sync word
        static_cast<int8_t>(CONFIG_LORA_TX_POWER), // tx power dBm
        static_cast<uint16_t>(CONFIG_LORA_PREAMBLE_LENGTH) // preamble length
    );
    if (initRes != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG_LORA, "LoRa RadioLib initialization failed (err=%d); skipping task startup", initRes);
        return;
    }

    // Enable CRC (2 bytes) and explicit header mode
    this->loraRadio->setCRC(2);
    this->loraRadio->explicitHeader();

    // Ensure the radio is in receive (interrupt) mode
    this->loraRadio->startReceive();

    ESP_LOGI(TAG_LORA, "LoRa configured: freq=%ld Hz, tx_power=%d, SF=%d, BW=%ld Hz, CR=4/%d, ping_interval=%d sec",
             CONFIG_LORA_FREQUENCY, CONFIG_LORA_TX_POWER, CONFIG_LORA_SPREADING_FACTOR,
             CONFIG_LORA_BANDWIDTH, CONFIG_LORA_CODING_RATE_DENOMINATOR, CONFIG_LORA_PING_INTERVAL);

    // Initialize last send time to now
    updateLastSendTime();

    if (receiveTaskHandle == nullptr) {
        BaseType_t created = xTaskCreate(
            &LoRa_CommunicationClass::receiveTaskEntry,
            "lora_rx_task",
            4096,
            this,
            tskIDLE_PRIORITY + 1,
            &receiveTaskHandle
        );
        if (created != pdPASS) {
            ESP_LOGE(TAG_LORA, "Failed to start LoRa receive task");
            receiveTaskHandle = nullptr;
            return;
        }
        // If a DIO0 pin is configured, install ISR service and attach handler to notify the receive task
#ifdef CONFIG_LORA_DIO0_PIN
        gpio_config_t io_conf{};
        io_conf.intr_type = GPIO_INTR_POSEDGE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = 1ULL << CONFIG_LORA_DIO0_PIN;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);

        if (gpio_install_isr_service(0) == ESP_OK) {
            if (gpio_isr_handler_add((gpio_num_t)CONFIG_LORA_DIO0_PIN, dio0_isr_handler,
                                     (void*)receiveTaskHandle) ==
                ESP_OK) {
                this->dio0IsrInstalled = true;
                ESP_LOGI(TAG_LORA, "DIO0 ISR installed on pin %d", CONFIG_LORA_DIO0_PIN);
            }
            else {
                ESP_LOGW(TAG_LORA, "Failed to add ISR handler for DIO0 pin %d", CONFIG_LORA_DIO0_PIN);
            }
        }
        else {
            ESP_LOGW(TAG_LORA, "Failed to install GPIO ISR service for DIO0");
        }
#endif
    }

    if (pingTaskHandle == nullptr) {
        BaseType_t created = xTaskCreate(
            &LoRa_CommunicationClass::pingTaskEntry,
            "lora_ping_task",
            4096,
            this,
            tskIDLE_PRIORITY + 1,
            &pingTaskHandle
        );
        if (created != pdPASS) {
            ESP_LOGE(TAG_LORA, "Failed to start LoRa ping task");
            pingTaskHandle = nullptr;
            return;
        }
    }

    if (callbackWorkerHandle == nullptr) {
        BaseType_t created = xTaskCreate(
            &LoRa_CommunicationClass::callbackWorkerEntry,
            "lora_callback_worker",
            4096,
            this,
            tskIDLE_PRIORITY + 1,
            &callbackWorkerHandle
        );
        if (created != pdPASS) {
            ESP_LOGE(TAG_LORA, "Failed to start LoRa callback worker task");
            callbackWorkerHandle = nullptr;
            return;
        }
    }

    if (joinFragmentsHandle == nullptr) {
        BaseType_t created = xTaskCreate(
            &LoRa_CommunicationClass::joinFragmentsEntry,
            "lora_join_fragments",
            4096,
            this,
            tskIDLE_PRIORITY + 1,
            &joinFragmentsHandle);
        if (created != pdPASS) {
            ESP_LOGE(TAG_LORA, "Failed to start LoRa join fragments task");
            joinFragmentsHandle = nullptr;
            return;
        }
    }

    ESP_LOGI(TAG_LORA, "LoRa communication initialized");
}


float LoRa_CommunicationClass::getLastPacketRSSI() const {
    return (loraRadio == nullptr) ? 0.0f : loraRadio->getRSSI(true);
}

float LoRa_CommunicationClass::getLastPacketSNR() const {
    return (loraRadio == nullptr) ? 0.0f : loraRadio->getSNR();
}

bool LoRa_CommunicationClass::sendData(const uint8_t* data, const size_t size) {
    if (data == nullptr || size == 0) {
        ESP_LOGE(TAG_LORA, "Invalid arguments passed");
        return false;
    }

    auto fragments = splitData(size);
    for (const auto& fragmentHeader : fragments) {
        size_t offset = fragmentHeader.fragmentId * LORA_MAX_DATA_LENGTH;
        ESP_LOGI(TAG_LORA, "Sending fragment %d/%d for messageId=%d with payload size %d",
                 fragmentHeader.fragmentId + 1, fragmentHeader.totalFragments, fragmentHeader.messageId,
                 fragmentHeader.payloadLength);
        if (!sendRawPacket(fragmentHeader, data + offset, true)) {
            return false;
        }
    }
    return true;
}

std::vector<LoRa_CommunicationClass::LoRa_Packet_Internal> LoRa_CommunicationClass::splitData(const size_t size) {
    std::vector<LoRa_Packet_Internal> fragments;

    size_t totalFragments = (size + LORA_MAX_DATA_LENGTH - 1) / LORA_MAX_DATA_LENGTH;
    uint8_t mid = nextMessageId.fetch_add(1);
    for (size_t i = 0; i < totalFragments; i++) {
        LoRa_Packet_Internal packetHeader = {};
        packetHeader.type = PacketType::HEADER;
        packetHeader.messageId = mid;
        packetHeader.payloadLength = std::min(size - i * LORA_MAX_DATA_LENGTH,
                                              static_cast<size_t>(LORA_MAX_DATA_LENGTH));
        packetHeader.fragmentId = i;
        packetHeader.totalFragments = totalFragments;
        fragments.push_back(packetHeader);
    }

    return fragments;
}

LoRa_CommunicationClass::ReceivedPacket LoRa_CommunicationClass::joinData(ReceivedFragmentsCache& fragmentCache) const {
    ReceivedPacket result{};
    result.header = fragmentCache.header;

    if (fragmentCache.fragments.empty()) {
        ESP_LOGW(TAG_LORA, "No fragments found for messageId=%d", fragmentCache.header.messageId);
        return result;
    }

    // Sort fragments by fragment ID
    std::ranges::sort(fragmentCache.fragments, [](const PacketFragment& a, const PacketFragment& b) {
        return a.fragmentId < b.fragmentId;
    });

    // Check if we have all fragments
    uint8_t totalFragments = result.header.totalFragments;
    if (fragmentCache.fragments.size() != totalFragments) {
        ESP_LOGW(TAG_LORA, "Missing fragments for messageId %u - expected %u, got %zu",
                 fragmentCache.header.messageId, totalFragments, fragmentCache.fragments.size());
        return result;
    }

    // Join payloads together
    size_t totalSize = 0;
    for (const auto& fragment : fragmentCache.fragments) {
        totalSize += fragment.payloadLength;
    }
    result.header.payloadLength = totalSize;
    result.payload = std::make_unique<uint8_t[]>(totalSize);
    size_t offset = 0;
    for (const auto& fragment : fragmentCache.fragments) {
        std::memcpy(result.payload.get() + offset, fragment.payload.get(), fragment.payloadLength);
        offset += fragment.payloadLength;
    }

    return result;
}

bool LoRa_CommunicationClass::sendRawPacket(LoRa_Packet_Internal packet, const uint8_t* data, bool requireAck) {
    const auto deviceId = DeviceId::get();
    std::memcpy(packet.senderId, deviceId.data(), sizeof(packet.senderId));

    // Helper: wait for ACK with timeout
    auto waitForAckId = [&](uint8_t msgId, time_t timeout) -> bool {
        vTaskDelay(pdMS_TO_TICKS(10)); // small initial
        time_t start = time(nullptr);
        while ((time(nullptr) - start) < timeout) {
            bool ackReceived = false;
            WITH_MUTEX(ackMutex) {
                // ACK received when msgId is no longer in outstandingAcks
                ackReceived = !std::ranges::contains(outstandingAcks, msgId);
            }
            if (ackReceived) {
                return true;
            }
            vTaskDelay(LORA_RX_POLL_DELAY);
        }
        return false;
    };

    // Helper: send a buffer with retry logic. Returns true if (no ACK required) or ack received.
    auto sendBufferWithRetries = [&](const std::unique_ptr<uint8_t[]>& buf, size_t len, uint8_t msgId, PacketType type) -> bool {
        if (len == 0) return true;
        if (len > LORA_MAX_PACKET_SIZE) {
            ESP_LOGE(TAG_LORA, "Packet too large: %zu > %zu", len, LORA_MAX_PACKET_SIZE);
            return false;
        }

        ESP_LOGD(TAG_LORA, "Transmitting packet msgId=%d with size %u", msgId, len);

        int attempts = 0;
        while (attempts <= LORA_MAX_SEND_RETRIES) {
            WITH_MUTEX(radioMutex) {
                sending = true;
                // transmit is blocking; ignore return value here but could be checked for errors
                loraRadio->transmit(buf.get(), len);
                // re-enter receive mode after transmit
                loraRadio->startReceive();
                sending = false;
            }

            if (!requireAck) return true;
            if (waitForAckId(msgId, LORA_ACK_TIMEOUT)) return true;

            ESP_LOGW(TAG_LORA, "No ACK for msgId=%d type=%d (attempt %d)", msgId, type, attempts);
            attempts++;
        }

        return false;
    };


    size_t sendSize = sizeof(LoRa_Packet_Internal) + packet.payloadLength;
    auto sendBuffer = std::make_unique<uint8_t[]>(sendSize);
    memcpy(sendBuffer.get(), &packet, sizeof(LoRa_Packet_Internal));
    if (packet.payloadLength > 0 && data != nullptr) {
        memcpy(sendBuffer.get() + sizeof(LoRa_Packet_Internal), data, packet.payloadLength);
    }

    // Add message ID to outstanding ACKs if ACK is required
    if (requireAck) {
        WITH_MUTEX(ackMutex) {
            outstandingAcks.push_back(packet.messageId);
        }
    }

    // Send header first
    ESP_LOGD(TAG_LORA, "Sending packet: %s", packet.toString().c_str());
    sentPackets.push_back(packet);
    auto success = sendBufferWithRetries(sendBuffer, sendSize, packet.messageId, packet.type);
    if (!success) {
        // Remove from outstanding ACKs if send failed
        WITH_MUTEX(ackMutex) {
            erase_if(outstandingAcks, [&packet](const uint8_t& id) { return id == packet.messageId; });
        }


        updateConnectionState(ConnectionState::CONNECTING);
        ESP_LOGE(TAG_LORA, "Failed to send packet after retries, giving up");
        return false;
    }

    updateConnectionState(ConnectionState::CONNECTED);
    updateLastSendTime();
    ESP_LOGD(TAG_LORA, "Packet msgId=%d sent successfully", packet.messageId);
    return true;
}

void LoRa_CommunicationClass::updateLastSendTime() {
    WITH_MUTEX(lastSendTimeMutex) {
        lastSendTime = time(nullptr);
    }
}

void LoRa_CommunicationClass::updateConnectionState(ConnectionState state) {
#ifdef FLIGHT_DEVICE_TYPE_PLANE
    FlightStorage.updateBaseConnectionState(state);
#endif
}

#ifdef FLIGHT_DEVICE_TYPE_BASE_STATION
void LoRa_CommunicationClass::updateConnectionState(uint32_t planeId, ConnectionState state) {
    FlightStorage.updatePlaneConnectionState(planeId, state);
}
#endif

std::string LoRa_CommunicationClass::toString(PacketType packetType) {
    switch (packetType) {
    case PacketType::HEADER: return "HEADER";
    case PacketType::ACK: return "ACK";
    case PacketType::PING: return "PING";
    default: return "UNKNOWN";
    }
}

std::string LoRa_CommunicationClass::LoRa_Packet_Internal::toString() const {
    char senderIdHex[9];
    snprintf(senderIdHex, sizeof(senderIdHex), "%02x%02x%02x%02x",
             senderId[0], senderId[1], senderId[2], senderId[3]);
    return "LoRa_Packet{type=" + LoRa_CommunicationClass::toString(type) +
        ", senderId=" + std::string(senderIdHex) +
        ", messageId=" + std::to_string(messageId) +
        ", payloadLength=" + std::to_string(payloadLength) + "}";
}


void LoRa_CommunicationClass::registerReceivePacketCallback(std::function<void(const LoRaPacket&)> callback) {
    receivePacketCallbacks.push_back(std::move(callback));
}

bool LoRa_CommunicationClass::isOwnPacket(const LoRa_Packet_Internal& packet) {
    // Check if the packet is one of our recently sent packets to avoid processing it as a received packet
    return std::ranges::find(sentPackets, packet) != sentPackets.end();
}

void LoRa_CommunicationClass::cleanupSendHistory() {
    if (sentPackets.size() <= SENT_PACKET_HISTORY_MAX) return;
    const size_t excess = sentPackets.size() - SENT_PACKET_HISTORY_MAX;
    ESP_LOGD(TAG_LORA, "Cleaning up sent packet history (currentCount=%zu, dropping oldest %zu)",
             sentPackets.size(), excess);
    // sentPackets is append-only (see sendRawPacket), so the oldest entries are at the front
    sentPackets.erase(sentPackets.begin(), sentPackets.begin() + excess);
}
