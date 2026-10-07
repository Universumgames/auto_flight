#include "LoRa_Communication.hpp"

#include "esp_log.h"
#include <algorithm>
#include <cstring>

#include "DeviceId.hpp"
#include "GPS_Reader.hpp"
#include "driver/gpio.h"
#include "esp_random.h"

#if FLIGHT_DEVICE_TYPE_BASE_STATION
#include "Cache.hpp"
#else
#include "FlightStorage.hpp"
#endif
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
    if (sendMutex == nullptr) {
        sendMutex = xSemaphoreCreateMutex();
    }
    if (radioMutex == nullptr || receivedPacketsMutex == nullptr || lastSendTimeMutex == nullptr || ackMutex ==
        nullptr || sendMutex == nullptr) {
        ESP_LOGE(TAG_LORA, "Failed to create LoRa mutexes");
        return;
    }

    outstandingAcks.reserve(20);

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
        static_cast<uint16_t>(CONFIG_LORA_PREAMBLE_LENGTH), // preamble length
        static_cast<float>(CONFIG_LORA_TCXO_VOLTAGE_MV) / 1000.0f // TCXO voltage in V
    );
    if (initRes != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG_LORA, "LoRa RadioLib initialization failed (err=%d); skipping task startup", initRes);
        return;
    }

    // RadioLib leaves the PA over-current protection at 60 mA, but the SX1262 draws ~90 mA at +17 dBm,
    // so the PA gets clamped and the transmitted signal is distorted. 140 mA is the SX1262 maximum.
    if (const int16_t res = this->loraRadio->setCurrentLimit(140.0f); res != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG_LORA, "Failed to set PA current limit (err=%d)", res);
    }
    // ~2 dB better sensitivity for ~0.5 mA more RX current
    if (const int16_t res = this->loraRadio->setRxBoostedGainMode(true); res != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG_LORA, "Failed to enable RX boosted gain (err=%d)", res);
    }

    // Enable CRC (2 bytes) and explicit header mode
    this->loraRadio->setCRC(2);
    this->loraRadio->explicitHeader();

    // Ensure the radio is in receive (interrupt) mode
    this->loraRadio->startReceive();

    // the peer may be busy sending a full-size packet before it gets to transmit our ACK
    maxPacketAirtimeMs = loraRadio->getTimeOnAir(LORA_MAX_PACKET_SIZE) / 1000;
    const uint32_t ackAirtimeMs = loraRadio->getTimeOnAir(sizeof(LoRa_Packet_Internal) + sizeof(LoRa_Packet_Internal::senderId)) / 1000;
    ackTimeoutMs = maxPacketAirtimeMs + ackAirtimeMs + LORA_ACK_MARGIN_MS;
    ESP_LOGI(TAG_LORA, "Max packet airtime=%lu ms, ACK timeout=%lu ms", maxPacketAirtimeMs, ackTimeoutMs);

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

    bool success = true;
    // hold the send lock for the whole message, so no other local packet gets in between the fragments
    WITH_MUTEX(sendMutex) {
        auto fragments = splitData(size);
        for (const auto& fragmentHeader : fragments) {
            size_t offset = fragmentHeader.fragmentId * LORA_MAX_DATA_LENGTH;
            ESP_LOGI(TAG_LORA, "Sending fragment %d/%d for messageId=%d with payload size %d",
                     fragmentHeader.fragmentId + 1, fragmentHeader.totalFragments, fragmentHeader.messageId,
                     fragmentHeader.payloadLength);
            if (!sendRawPacket(fragmentHeader, data + offset, true)) {
                success = false;
                break;
            }
            vTaskDelay(LORA_SEND_DELAY);
        }
    }
    return success;
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
    ReceivedPacket result{
        .header = {
            .type = fragmentCache.header.type,
            .senderId = {
                fragmentCache.header.senderId[0], fragmentCache.header.senderId[1],
                fragmentCache.header.senderId[2], fragmentCache.header.senderId[3]
            },
            .messageId = fragmentCache.header.messageId,
            .totalFragments = fragmentCache.header.totalFragments,
            .payloadLength = 0, // will be calculated after joining
        },
        .payload = nullptr
    };

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
    const OutstandingAck ackKey{packet.messageId, packet.fragmentId};
    auto waitForAck = [&](uint32_t timeoutMs) -> bool {
        TickType_t start = xTaskGetTickCount();
        while (pdTICKS_TO_MS(xTaskGetTickCount() - start) < timeoutMs) {
            bool ackReceived = false;
            WITH_MUTEX(ackMutex) {
                // ACK received when the packet is no longer in outstandingAcks
                ackReceived = !std::ranges::contains(outstandingAcks, ackKey);
            }
            if (ackReceived) {
                return true;
            }
            vTaskDelay(LORA_RX_POLL_DELAY);
        }
        return false;
    };

    // Helper: send a buffer with retry logic. Returns true if (no ACK required) or ack received.
    auto sendBufferWithRetries = [&](const std::unique_ptr<uint8_t[]>& buf, size_t len, uint8_t msgId,
                                     PacketType type) -> bool {
        if (len == 0) return true;
        if (len > LORA_MAX_PACKET_SIZE) {
            ESP_LOGE(TAG_LORA, "Packet too large: %zu > %zu", len, LORA_MAX_PACKET_SIZE);
            return false;
        }

        ESP_LOGD(TAG_LORA, "Transmitting packet msgId=%d with size %u", msgId, len);

        // the receive task sends ACKs right after reading a packet; it must not wait for itself to read the next one
        const bool deferToReceiver = xTaskGetCurrentTaskHandle() != receiveTaskHandle;

        int attempts = 0;
        while (attempts <= LORA_MAX_SEND_RETRIES) {
            const bool transmitted = transmitWhenChannelClear(buf.get(), len, deferToReceiver);

            if (!requireAck) return transmitted;
            if (transmitted && waitForAck(ackTimeoutMs + esp_random() % maxPacketAirtimeMs)) return true;

            ESP_LOGW(TAG_LORA, "No ACK for msgId=%d fragment=%d type=%d (attempt %d)", msgId, ackKey.fragmentId,
                     type, attempts);
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
            outstandingAcks.push_back(ackKey);
        }
    }

    // Send header first
    ESP_LOGD(TAG_LORA, "Sending packet: %s", packet.toString().c_str());
    auto success = sendBufferWithRetries(sendBuffer, sendSize, packet.messageId, packet.type);
    if (!success) {
        // Remove from outstanding ACKs if send failed
        WITH_MUTEX(ackMutex) {
            std::erase(outstandingAcks, ackKey);
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

bool LoRa_CommunicationClass::transmitWhenChannelClear(const uint8_t* buf, const size_t len,
                                                       const bool deferToReceiver) {
    const TickType_t start = xTaskGetTickCount();
    while (true) {
        bool done = false;
        int16_t res = RADIOLIB_ERR_NONE;
        WITH_MUTEX(radioMutex) {
            const uint32_t irq = loraRadio->getIrqFlags();
            // HEADER_VALID without a read yet: a packet is arriving right now; RX_DONE: one is waiting in the FIFO
            const bool receiving = irq & (RADIOLIB_SX126X_IRQ_HEADER_VALID | RADIOLIB_SX126X_IRQ_RX_DONE);
            const TickType_t now = xTaskGetTickCount();
            const bool waitedTooLong = pdTICKS_TO_MS(now - start) > maxPacketAirtimeMs + LORA_ACK_MARGIN_MS;
            // both are bounded by the receive task itself, so no waitedTooLong escape is needed
            const bool reserved = ackPending
                || static_cast<int32_t>(channelReservedUntil.load() - now) > 0;
            if (!deferToReceiver || (!reserved && (!receiving || waitedTooLong))) {
                if (receiving) {
                    ESP_LOGW(TAG_LORA, "Transmitting while a packet is being received (irq=0x%04lx)", irq);
                }
                sending = true;
                res = loraRadio->transmit(buf, len);
                // re-enter receive mode after transmit
                loraRadio->startReceive();
                sending = false;
                done = true;
            }
        }
        if (done) {
            if (res != RADIOLIB_ERR_NONE) {
                ESP_LOGE(TAG_LORA, "Transmit failed (err=%d)", res);
            }
            return res == RADIOLIB_ERR_NONE;
        }
        vTaskDelay(LORA_RX_POLL_DELAY);
    }
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
    if (state == ConnectionState::CONNECTED)
        Cache.updateSourceLastUpdateTime(planeId, GPS_Reader.getGPSLatestTime());
    else Cache.updateSourceLastUpdateTime(planeId, 0);
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

bool LoRa_CommunicationClass::isDuplicateFragment(const LoRa_Packet_Internal& header) {
    RecentRx entry{
        .valid = true,
        .senderId = {header.senderId[0], header.senderId[1], header.senderId[2], header.senderId[3]},
        .messageId = header.messageId,
        .fragmentId = header.fragmentId,
    };
    if (std::ranges::contains(recentRx, entry)) {
        return true;
    }
    recentRx[recentRxNext] = entry;
    recentRxNext = (recentRxNext + 1) % recentRx.size();
    return false;
}
