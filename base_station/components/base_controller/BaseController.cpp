#include "BaseController.hpp"

#include "Barometer.hpp"
#include "Battery.hpp"
#include "GPS_Reader.hpp"
#include "i2c_manager.hpp"
#include "LoRa_Communication.hpp"
#include "FrontendBl.hpp"
#include "OledDisplay.hpp"
#include "Cache.hpp"
#include "Flight_Communication.hpp"
#include "packets/component.hpp"
#include "packets/sensor.hpp"
#include <cinttypes>

BaseControllerClass* instance = nullptr;

BaseControllerClass& BaseController = BaseControllerClass::getInstance();

const char* BaseControllerClass::TAG_BASE_CONTROLLER = "BaseController";

BaseControllerClass* BaseControllerClass::getInstancePtr() {
    if (instance == nullptr) {
        instance = new BaseControllerClass();
    }
    return instance;
}

BaseControllerClass& BaseControllerClass::getInstance() {
    return *getInstancePtr();
}


void BaseControllerClass::init() {
    I2CManager::getBus(); // initialize I2C bus

    Cache.init();
    FlightStorage.init();

    FrontendHandlerBl.init();

    LoRa_Communication.begin();

    Barometer.begin();
    Battery.begin();
    GPS_Reader.begin();
    OledDisplay.begin();

    xTaskCreate(loopTaskEntry, "BaseControllerLoop", 4096, this, tskIDLE_PRIORITY + 1, nullptr);

    LoRa_Communication.registerReceivePacketCallback([this](const LoRaPacket packet) {
        communicationCallback(packet);
    });

    Cache.registerPacketCallback([](uint32_t sourceId, PacketType type, RawSerializedPacket data, size_t len) {
        if (type == PacketType::PLANNED_AREA) {
            ESP_LOGI(TAG_BASE_CONTROLLER, "Received planned area update from plane %u", sourceId);
            LoRa_Communication.sendData(data, len);
        }
    });
}


void BaseControllerClass::loopTaskEntry(void* param) {
    auto* instance = static_cast<BaseControllerClass*>(param);
    instance->loopTask();
}

[[noreturn]] void BaseControllerClass::loopTask() {
    while (true) {
        const auto time = GPS_Reader.getGPSLatestTime();
        auto sensor = SensorUpdate{
            time,
            Barometer.getPressure(),
            0,
            BatteryClass::voltageToPercentage(Battery.getVoltageMillivolts())
        };
        Cache.savePacket(DeviceId::BASE_STATION, PacketType::SENSOR_UPDATE, sensor.serialize());

        auto connection = ComponentStatus{
            time,
            GPS_Reader.hasValidPosition() ? ConnectionState::CONNECTED : ConnectionState::CONNECTING,
            Barometer.available() ? ConnectionState::CONNECTED : ConnectionState::CONNECTING,
            ConnectionState::CONNECTING,
            ConnectionState::CONNECTING,
            ConnectionState::CONNECTING,
            false,
            FlightState::PLANNING
        };
        Cache.savePacket(DeviceId::BASE_STATION, PacketType::COMPONENT_STATUS, connection.serialize());

        auto position = PositionUpdate{
            time,
            GPS_Reader.getCurrentPosition()
        };
        Cache.savePacket(DeviceId::BASE_STATION, PacketType::POSITION, position.serialize());

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void BaseControllerClass::communicationCallback(LoRaPacket packet) {
    auto basePacket = BasePacket(packet.payload, packet.length);

    ESP_LOGI(TAG_BASE_CONTROLLER, "Received packet of length %u with type 0x%02x at time %" PRIi64 " from %" PRIu32,
             packet.length, basePacket.type, basePacket.timestamp, basePacket.id);
    if (basePacket.id == DeviceId::BASE_STATION) {
        ESP_LOGE(TAG_BASE_CONTROLLER, "Received packet supposedly from self");
    }

    if (!Cache.hasSource(basePacket.id)) {
        ESP_LOGI(TAG_BASE_CONTROLLER, "New source detected: %" PRIu32, basePacket.id);
    }

    auto data = std::make_unique<uint8_t[]>(packet.length);
    std::memcpy(data.get(), packet.payload, packet.length);

    Cache.savePacket(basePacket.id, basePacket.type, {std::move(data), packet.length});
}
