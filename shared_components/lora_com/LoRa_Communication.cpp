#include "LoRa_Communication.hpp"

#include "esp_log.h"
#include "lora.hpp"
#include <algorithm>
#include <cstring>

#include "driver/gpio.h"

#include "FlightStorage.hpp"
#include "helper.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

const char* TAG_LORA = "LoRa_Communication";
constexpr size_t LORA_MAX_PACKET_SIZE = 255;

constexpr TickType_t LORA_RX_POLL_DELAY = pdMS_TO_TICKS(10);
constexpr TickType_t LORA_ACK_TIMEOUT = pdMS_TO_TICKS(3000);
constexpr TickType_t LORA_PING_CHECK_INTERVAL = pdMS_TO_TICKS(1000);  // Check every 1 second
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
    if (radioMutex == nullptr || receivedPacketsMutex == nullptr || lastSendTimeMutex == nullptr || ackMutex == nullptr) {
        ESP_LOGE(TAG_LORA, "Failed to create LoRa mutexes");
        return;
    }

    ESP_LOGI(TAG_LORA, "initializing LoRa radio");


    if (!lora_init()) {
        ESP_LOGE(TAG_LORA, "LoRa SPI/radio initialization failed; skipping task startup");
        return;
    }
    lora_set_frequency(CONFIG_LORA_FREQUENCY);
    lora_set_tx_power(CONFIG_LORA_TX_POWER);
    lora_set_spreading_factor(CONFIG_LORA_SPREADING_FACTOR);
    lora_set_bandwidth(CONFIG_LORA_BANDWIDTH);
    lora_set_coding_rate(CONFIG_LORA_CODING_RATE_DENOMINATOR);
    lora_set_preamble_length(CONFIG_LORA_PREAMBLE_LENGTH);
    lora_enable_crc();
    lora_explicit_header_mode();
    lora_set_sync_word(CONFIG_LORA_SYNC_WORD);
    lora_receive();

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
            if (gpio_isr_handler_add((gpio_num_t)CONFIG_LORA_DIO0_PIN, dio0_isr_handler, (void*)receiveTaskHandle) == ESP_OK) {
                this->dio0IsrInstalled = true;
                ESP_LOGI(TAG_LORA, "DIO0 ISR installed on pin %d", CONFIG_LORA_DIO0_PIN);
            } else {
                ESP_LOGW(TAG_LORA, "Failed to add ISR handler for DIO0 pin %d", CONFIG_LORA_DIO0_PIN);
            }
        } else {
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


int LoRa_CommunicationClass::getLastPacketRSSI() const {
    return lora_packet_rssi();
}

float LoRa_CommunicationClass::getLastPacketSNR() const {
    return lora_packet_snr();
}

bool LoRa_CommunicationClass::hasReceivedData() const {
    if (receivedPacketsMutex == nullptr) {
        return false;
    }

    bool hasData = false;
    if (xSemaphoreTake(receivedPacketsMutex, portMAX_DELAY) == pdTRUE) {
        hasData = !receivedPackets.empty();
        xSemaphoreGive(receivedPacketsMutex);
    }
    return hasData;
}

int LoRa_CommunicationClass::receiveData(uint8_t* buffer, int size) {
    if (buffer == nullptr || size <= 0 || receivedPacketsMutex == nullptr) {
        return 0;
    }

    ReceivedPacket packet = {};
    bool hasPacket = false;
    if (xSemaphoreTake(receivedPacketsMutex, portMAX_DELAY) != pdTRUE) {
        return 0;
    }

    if (!receivedPackets.empty()) {
        packet = std::move(receivedPackets.front());
        receivedPackets.erase(receivedPackets.begin());
        hasPacket = true;
    }
    xSemaphoreGive(receivedPacketsMutex);

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

int LoRa_CommunicationClass::sendRawPacket(const LoRa_Packet& packet, const uint8_t* data, bool requireAck) {
    if (radioMutex == nullptr) {
        return -1;
    }
    // Helper: wait for ACK with timeout
    auto waitForAckId = [&](uint8_t msgId, TickType_t timeout) -> bool {
        TickType_t start = xTaskGetTickCount();
        while ((xTaskGetTickCount() - start) < timeout) {
            if (ackMutex != nullptr && xSemaphoreTake(ackMutex, portMAX_DELAY) == pdTRUE) {
                auto it = std::find(receivedAcks.begin(), receivedAcks.end(), msgId);
                if (it != receivedAcks.end()) {
                    receivedAcks.erase(it);
                    xSemaphoreGive(ackMutex);
                    return true;
                }
                xSemaphoreGive(ackMutex);
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
            if (xSemaphoreTake(radioMutex, portMAX_DELAY) != pdTRUE) {
                ESP_LOGE(TAG_LORA, "Failed to take radio mutex for sending");
                break;
            }
            lora_send_packet(const_cast<uint8_t*>(buf), static_cast<uint8_t>(len));
            lora_receive();
            xSemaphoreGive(radioMutex);

            if (!requireAck) return true;
            if (waitForAckId(msgId, LORA_ACK_TIMEOUT)) return true;

            attempts++;
            ESP_LOGW(TAG_LORA, "No ACK for msgId=%d (attempt %d)", msgId, attempts);
        }
        return false;
    };

    // Send header first
    if (!sendBufferWithRetries(reinterpret_cast<const uint8_t*>(&packet), sizeof(packet), packet.messageId)) {
        lora_receive();
        updateConnectionState(ConnectionState::CONNECTING);
        return -1;
    }

    // Send payload if present
    if (packet.payloadLength > 0 && data != nullptr) {
        if (!sendBufferWithRetries(data, packet.payloadLength, packet.messageId)) {
            lora_receive();
            return -1;
        }
    }

    lora_receive();
    return 0;
}

void LoRa_CommunicationClass::updateLastSendTime() {
    if (lastSendTimeMutex == nullptr) {
        return;
    }

    if (xSemaphoreTake(lastSendTimeMutex, portMAX_DELAY) == pdTRUE) {
        lastSendTime = xTaskGetTickCount();
        xSemaphoreGive(lastSendTimeMutex);
    }
}

void LoRa_CommunicationClass::updateConnectionState(ConnectionState state) {
    if (isDeviceBaseStation()) {
        FlightStorage.updatePlaneConnectionState(state);
    }else if (isDevicePlane()) {
        FlightStorage.updateBaseStationConnectionState(state);
    }
}

std::string LoRa_CommunicationClass::LoRa_Packet::toString() const {
    return "LoRa_Packet{type=" + std::to_string(static_cast<int>(type)) +
           ", messageId=" + std::to_string(messageId) +
           ", payloadLength=" + std::to_string(payloadLength) + "}";
}
