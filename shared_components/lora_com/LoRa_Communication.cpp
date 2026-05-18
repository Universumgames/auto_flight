#include "LoRa_Communication.hpp"

#include "esp_log.h"
#include <algorithm>
#include <cstring>

#include "driver/gpio.h"

#include "FlightStorage.hpp"
#include "helper.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "EspHal.h"
#include "mutex_helper.hpp"

const char* TAG_LORA = "LoRa_Communication";
constexpr size_t LORA_MAX_PACKET_SIZE = 255;

constexpr TickType_t LORA_RX_POLL_DELAY = pdMS_TO_TICKS(10);
constexpr TickType_t LORA_ACK_TIMEOUT = pdMS_TO_TICKS(3000);
constexpr TickType_t LORA_PING_CHECK_INTERVAL = pdMS_TO_TICKS(1000); // Check every 1 second
constexpr TickType_t LORA_PAYLOAD_TIMEOUT = pdMS_TO_TICKS(3000);
constexpr int LORA_MAX_SEND_RETRIES = 3;

static LoRa_CommunicationClass* lo_ra_communication = nullptr;

LoRa_CommunicationClass& LoRa_Communication = LoRa_CommunicationClass::getInstance();

LoRa_CommunicationClass* LoRa_CommunicationClass::getInstancePtr() {
    if (lo_ra_communication == nullptr) {
        lo_ra_communication = new LoRa_CommunicationClass();
    }
    return lo_ra_communication;
}

static void IRAM_ATTR dio0_isr_handler(void* arg) {
    // arg is expected to be a TaskHandle_t passed during registration
    if (arg == nullptr) return;
    auto task = static_cast<TaskHandle_t>(arg);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(task, &xHigherPriorityTaskWoken);
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

    ESP_LOGI(TAG_LORA, "initializing LoRa radio");

    // Construct RadioLib HAL/Module and SX1262 instance
    auto* hal = new EspHal(CONFIG_LORA_SCK_GPIO, CONFIG_LORA_MISO_GPIO, CONFIG_LORA_MOSI_GPIO);
    auto module = new Module(hal, CONFIG_LORA_CS_GPIO, CONFIG_LORA_DIO0_PIN, CONFIG_LORA_RST_GPIO, CONFIG_LORA_BUSY_GPIO);
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
            if (gpio_isr_handler_add((gpio_num_t)CONFIG_LORA_DIO0_PIN, dio0_isr_handler, (void*)receiveTaskHandle) ==
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
            2048,
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

    ESP_LOGI(TAG_LORA, "LoRa communication initialized");
}


float LoRa_CommunicationClass::getLastPacketRSSI() const {
    return (loraRadio == nullptr) ? 0.0f : loraRadio->getRSSI(true);
}

float LoRa_CommunicationClass::getLastPacketSNR() const {
    return (loraRadio == nullptr) ? 0.0f : loraRadio->getSNR();
}

bool LoRa_CommunicationClass::hasReceivedData() const {

    bool hasData = false;
    WITH_MUTEX(receivedPacketsMutex) {
        hasData = !receivedPackets.empty();
    }
    return hasData;
}

int LoRa_CommunicationClass::receiveData(uint8_t* buffer, int size) {
    if (buffer == nullptr || size <= 0) {
        return 0;
    }

    ReceivedPacket packet = {};
    bool hasPacket = false;
    WITH_MUTEX(receivedPacketsMutex) {
        if (!receivedPackets.empty()) {
            packet = std::move(receivedPackets.front());
            receivedPackets.erase(receivedPackets.begin());
            hasPacket = true;
        }
    }

    if (!hasPacket || packet.header.type != PacketType::HEADER) {
        return 0;
    }

    int copyLen = std::min(size, static_cast<int>(packet.header.payloadLength));
    if (copyLen > 0 && packet.payload != nullptr) {
        std::memcpy(buffer, packet.payload.get(), static_cast<size_t>(copyLen));
    }
    return copyLen;
}

void LoRa_CommunicationClass::sendData(const uint8_t* data, uint8_t size) {
    LoRa_Packet packetHeader = {};
    packetHeader.type = PacketType::HEADER;
    // generate a non-zero message id
    uint8_t mid = nextMessageId.fetch_add(1);
    if (mid == 0) mid = nextMessageId.fetch_add(1);
    packetHeader.messageId = mid;
    packetHeader.payloadLength = size; // size of next data packet
    sendRawPacket(packetHeader, data, true);
    updateLastSendTime();
}

void LoRa_CommunicationClass::sendAck(uint8_t messageId) {
    LoRa_Packet ackPacket = {
        .type = PacketType::ACK,
        .messageId = messageId,
        .payloadLength = 0,
    };
    sendRawPacket(ackPacket, nullptr, false);
    updateLastSendTime();
}

bool LoRa_CommunicationClass::sendRawPacket(const LoRa_Packet& packet, const uint8_t* data, bool requireAck) {
    // Helper: wait for ACK with timeout
    auto waitForAckId = [&](uint8_t msgId, TickType_t timeout) -> bool {
        TickType_t start = xTaskGetTickCount();
        while ((xTaskGetTickCount() - start) < timeout) {
            auto found = false;
            WITH_MUTEX(ackMutex) {
                auto it = std::ranges::find(receivedAcks, msgId);
                found = it != receivedAcks.end();
            }
            if (found) {
                return true;
            }
            vTaskDelay(LORA_RX_POLL_DELAY);
        }
        return false;
    };

    // Helper: send a buffer with retry logic. Returns true if (no ACK required) or ack received.
    auto sendBufferWithRetries = [&](const uint8_t* buf, size_t len, uint8_t msgId) -> bool {
        if (len == 0) return true;
        if (len > LORA_MAX_PACKET_SIZE) {
            ESP_LOGE(TAG_LORA, "Packet too large: %zu > %zu", len, LORA_MAX_PACKET_SIZE);
            return false;
        }

        int attempts = 0;
        while (attempts <= LORA_MAX_SEND_RETRIES) {
            WITH_MUTEX(radioMutex) {
                // transmit is blocking; ignore return value here but could be checked for errors
                loraRadio->transmit(buf, len);
                // re-enter receive mode after transmit
                loraRadio->startReceive();
            }

            if (!requireAck) return true;
            if (waitForAckId(msgId, LORA_ACK_TIMEOUT)) return true;

            attempts++;
            ESP_LOGW(TAG_LORA, "No ACK for msgId=%d (attempt %d)", msgId, attempts);
        }
        return false;
    };

    // Send header first
    if (!sendBufferWithRetries(reinterpret_cast<const uint8_t*>(&packet), sizeof(packet), packet.messageId)) {
        loraRadio->startReceive();
        updateConnectionState(ConnectionState::CONNECTING);
        return false;
    }

    // Send payload if present
    if (packet.payloadLength > 0 && data != nullptr) {
        if (!sendBufferWithRetries(data, packet.payloadLength, packet.messageId)) {
            loraRadio->startReceive();
            return false;
        }
    }
    loraRadio->startReceive();
    return true;
}

void LoRa_CommunicationClass::updateLastSendTime() {
    WITH_MUTEX(lastSendTimeMutex) {
        lastSendTime = xTaskGetTickCount();
    }
}

void LoRa_CommunicationClass::updateConnectionState(ConnectionState state) {
    if (isDeviceBaseStation()) {
        FlightStorage.updatePlaneConnectionState(state);
    }
    else if (isDevicePlane()) {
        FlightStorage.updateBaseStationConnectionState(state);
    }
}

std::string LoRa_CommunicationClass::toString(PacketType packetType) {
    switch (packetType) {
        case PacketType::HEADER: return "HEADER";
        case PacketType::ACK: return "ACK";
        case PacketType::PING: return "PING";
        default: return "UNKNOWN";
    }
}

std::string LoRa_CommunicationClass::LoRa_Packet::toString() const {
    return "LoRa_Packet{type=" + LoRa_CommunicationClass::toString(type) +
        ", messageId=" + std::to_string(messageId) +
        ", payloadLength=" + std::to_string(payloadLength) + "}";
}
