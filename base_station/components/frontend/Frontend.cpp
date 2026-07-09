#include "Frontend.hpp"

#include "esp_http_server.h"
#include "FlightStorage.hpp"
#include "GPS_Reader.hpp"
#include "Barometer.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ESP_ERROR_CHECK_SOFT(err) \
if(err != ESP_OK) { \
ESP_ERROR_CHECK_WITHOUT_ABORT(err); \
return err; \
}

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"


static FrontendHandlerClass* instance = nullptr;
const char* FrontendHandlerClass::TAG_FRONTEND = "Frontend";

FrontendHandlerClass& FrontendHandler = FrontendHandlerClass::getInstance();

FrontendHandlerClass* FrontendHandlerClass::getInstancePtr() {
    if (instance == nullptr) {
        instance = new FrontendHandlerClass();
    }
    return instance;
}

FrontendHandlerClass& FrontendHandlerClass::getInstance() {
    return *getInstancePtr();
}

FrontendHandlerClass::FrontendHandlerClass() = default;

void FrontendHandlerClass::init() {
    ESP_LOGI(TAG_FRONTEND, "Initializing frontend handler");
    config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    // Reduce stack size to allow more concurrent connections
    config.stack_size = 4096;
    // Increase backlog to handle connection requests better
    config.backlog_conn = 16;
    // Increase max open connections
    config.max_open_sockets = 10;
    config.task_priority = tskIDLE_PRIORITY + 2;

    esp_err_t err;
    err = httpd_start(&httpd_handle, &config);
    ESP_ERROR_CHECK(err);

    ESP_LOGI(TAG_FRONTEND, "Mounting filesystem");
    mountFS();

    ESP_LOGI(TAG_FRONTEND, "Registering URI Handler");
    registerURIHandlers();
    // start a FreeRTOS task to send websocket updates every second
    xTaskCreate(
        sendWSTaskEntry,
        "ws_update_task",
        4096,
        nullptr,
        tskIDLE_PRIORITY + 1,
        nullptr
    );
    ESP_LOGI(TAG_FRONTEND, "Frontend handler initialized");
}


void FrontendHandlerClass::registerURIHandlers() {
    registerPing();
    registerWebsocket();

    registerAPISockets();

    // always has to be called last
    registerStaticURIHandler();
}

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

void FrontendHandlerClass::registerPing() {
    ESP_LOGI(TAG_FRONTEND, "Registering ping URI handler");
    static httpd_uri_t ping_uri = {
        .uri = "/api/ping",
        .method = HTTP_GET,
        .handler = [](httpd_req_t* req) -> esp_err_t {
            ESP_LOGD(TAG_FRONTEND, "Received ping request");
            const char* resp_str = "pong";
            esp_err_t ret = httpd_resp_send(req, resp_str, strlen(resp_str));
            if (ret != ESP_OK) {
                ESP_LOGE(TAG_FRONTEND, "Failed to send ping response: %s", esp_err_to_name(ret));
            }
            return ret;
        },
        .user_ctx = nullptr,
        .is_websocket = false,
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &ping_uri));
}

httpd_ws_frame_t* FrontendHandlerClass::prepareFlightPacket() {
    FlightUpdatePacket rawPacket = {
        .basePosition= FlightStorage.getBasePosition(),
        .basePositionUpdateTime = FlightStorage.getLastBasePositionUpdateTime(),
        .planePosition = FlightStorage.getPlanePosition(),
        .planePositionUpdateTime = FlightStorage.getLastPlanePositionUpdateTime(),
        .flightRoute = FlightStorage.getFlightRoute(),
        .flightRouteUpdateTime = FlightStorage.getLastFlightRouteUpdateTime(),
        .plannedRoute = FlightStorage.getPlannedRoute(),
        .plannedRouteUpdateTime = FlightStorage.getLastPlannedRouteUpdateTime(),
    };
    // encode to json
    nlohmann::json json = rawPacket;
    std::string jsonString = json.dump();

    httpd_ws_frame_t * ws_pkt = new httpd_ws_frame_t();
    ws_pkt->type = HTTPD_WS_TYPE_TEXT;
    // allocate persistent payload so async worker can copy from a valid buffer
    uint8_t* payload = (uint8_t*)malloc(jsonString.size() + 1);
    if (!payload) {
        delete ws_pkt;
        return nullptr;
    }
    memcpy(payload, jsonString.c_str(), jsonString.size() + 1);
    ws_pkt->payload = payload;
    ws_pkt->len = jsonString.size() + 1;
    return ws_pkt;
}

void FrontendHandlerClass::sendWSUpdate() {
    auto flightPkt = prepareFlightPacket();
    if (flightPkt != nullptr) {
        broadcastWSPacket(flightPkt);
        // free the payload we allocated in prepareFlightPacket
        free(flightPkt->payload);
        delete flightPkt;
    }
    auto connectionPkt = prepareConnectionPacket();
    if (connectionPkt != nullptr) {
        broadcastWSPacket(connectionPkt);
        free(connectionPkt->payload);
        delete connectionPkt;
    }
    auto sensorPkt = prepareSensorPacket();
    if (sensorPkt != nullptr) {
        broadcastWSPacket(sensorPkt);
        free(sensorPkt->payload);
        delete sensorPkt;
    }
}

#define CONNECTION_STATE_BY_LAST_UPDATE_TIME(updateTime, timeout)\
    (GPS_Reader.getGPSLatestTime() - (updateTime) < (timeout) ? ConnectionState::CONNECTED : ConnectionState::CONNECTING)

httpd_ws_frame_t* FrontendHandlerClass::prepareConnectionPacket() {
    ConnectionUpdatePacket rawPacket = {
        .baseConnectionState = ConnectionState::CONNECTED,
        .lastContactBaseStationTimestamp = GPS_Reader.getGPSLatestTime(),
        .planeConnectionState = FlightStorage.getPlaneConnectionState(),
        .lastContactPlaneTimestamp = FlightStorage.getLastPlaneConnectionStateUpdateTime(),
        .gpsConnectionBase = GPS_Reader.hasValidPosition() ? ConnectionState::CONNECTED : ConnectionState::CONNECTING,
        .gpsConnectionPlane = FlightStorage.getPlaneGPSConnectionState(),
        .barometerConnectionBase = FlightStorage.getBaseBarometerConnectionState(),
        .barometerConnectionPlane = FlightStorage.getPlaneBarometerConnectionState(),
        .motorComConnectionPlane = FlightStorage.getPlaneMotorControlConnectionState(),
        .magnetometerConnectionPlane = FlightStorage.getPlaneMagnetometerConnectionState(),
        .accelerometerConnectionPlane = FlightStorage.getPlaneAccelerometerConnectionState(),
        .manualOverridePlane = FlightStorage.getPlaneManualOverride(),
    };
    nlohmann::json json = rawPacket;
    std::string jsonString = json.dump();
    httpd_ws_frame_t* ws_pkt = new httpd_ws_frame_t();
    ws_pkt->type = HTTPD_WS_TYPE_TEXT;
    uint8_t* payload = (uint8_t*)malloc(jsonString.size() + 1);
    if (!payload) {
        delete ws_pkt;
        return nullptr;
    }
    memcpy(payload, jsonString.c_str(), jsonString.size() + 1);
    ws_pkt->payload = payload;
    ws_pkt->len = jsonString.size() + 1;
    return ws_pkt;
}

httpd_ws_frame_t* FrontendHandlerClass::prepareSensorPacket() {
    SensorPacket rawPacket = {
        .barometerPressureBase = FlightStorage.getBasePressure(),
        .barometerPressurePlane = FlightStorage.getPlanePressure(),
        .calculatedAltitude = Barometer.calculateAltitude(FlightStorage.getBasePressure(), FlightStorage.getPlanePressure()),
    };
    nlohmann::json json = rawPacket;
    std::string jsonString = json.dump();
    httpd_ws_frame_t* ws_pkt = new httpd_ws_frame_t();
    ws_pkt->type = HTTPD_WS_TYPE_TEXT;
    uint8_t* payload = (uint8_t*)malloc(jsonString.size() + 1);
    if (!payload) {
        delete ws_pkt;
        return nullptr;
    }
    memcpy(payload, jsonString.c_str(), jsonString.size() + 1);
    ws_pkt->payload = payload;
    ws_pkt->len = jsonString.size() + 1;
    return ws_pkt;
}

void FrontendHandlerClass::registerAPISockets() {
    ESP_LOGI(TAG_FRONTEND, "Registering API URI handlers");
    ESP_LOGI(TAG_FRONTEND, "Registering /api/status handler");
    static httpd_uri_t status_uri = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = [](httpd_req_t* req) -> esp_err_t {
            ConnectionUpdatePacket rawPacket = {
                .baseConnectionState = ConnectionState::CONNECTED,
                .lastContactBaseStationTimestamp = GPS_Reader.getGPSLatestTime(),
                .planeConnectionState = FlightStorage.getPlaneConnectionState(),
                .lastContactPlaneTimestamp = FlightStorage.getLastPlaneConnectionStateUpdateTime(),
                .gpsConnectionBase = GPS_Reader.hasValidPosition() ? ConnectionState::CONNECTED : ConnectionState::CONNECTING,
                .gpsConnectionPlane = FlightStorage.getPlaneGPSConnectionState(),
                .barometerConnectionBase = FlightStorage.getBaseBarometerConnectionState(),
                .barometerConnectionPlane = FlightStorage.getPlaneBarometerConnectionState(),
                .motorComConnectionPlane = FlightStorage.getPlaneMotorControlConnectionState(),
                .magnetometerConnectionPlane = FlightStorage.getPlaneMagnetometerConnectionState(),
                .accelerometerConnectionPlane = FlightStorage.getPlaneAccelerometerConnectionState(),
                .manualOverridePlane = FlightStorage.getPlaneManualOverride(),
            };
            nlohmann::json json = rawPacket;
            std::string jsonString = json.dump();

            return httpd_resp_send(req, jsonString.c_str(), jsonString.size() + 1);
        },
        .user_ctx = nullptr,
        .is_websocket = false,
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &status_uri));

    ESP_LOGI(TAG_FRONTEND, "Registering /api/area handler");
    static httpd_uri_t area_uri = {
        .uri = "/api/area",
        .method = HTTP_POST,
        .handler = [](httpd_req_t* req) -> esp_err_t {
            auto strLen = req->content_len + 1;
            auto body = req->content_len > 0 ? std::make_unique<char[]>(strLen) : nullptr;
            int returnNr = -1;
            if (body) {
                int ret = httpd_req_recv(req, body.get(), strLen);
                if (ret <= 0) {
                    return ESP_FAIL;
                }
                ESP_LOGI(TAG_FRONTEND, "Received body: %d %s", req->content_len, body.get());
                auto shape = nlohmann::json::parse(body.get()).get<AreaDefinePacket>().shape;
                body.release();
                FlightStorage.updatePlannedArea(shape);
                returnNr = shape.size();
            }
            auto retStr = std::to_string(returnNr);
            return httpd_resp_send(req, retStr.c_str(), retStr.size());
        }
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &area_uri));

    static httpd_uri_t area_get_uri = {
        .uri = "/api/area",
        .method = HTTP_GET,
        .handler = [](httpd_req_t* req) -> esp_err_t {
            AreaDefinePacket areaPacket = {
                .shape = FlightStorage.getPlannedArea()
            };
            nlohmann::json json = areaPacket;
            std::string jsonString = json.dump();
            return httpd_resp_send(req, jsonString.c_str(), jsonString.size());
        }
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &area_get_uri));

    ESP_LOGI(TAG_FRONTEND, "Registering /api/route handler");
    static httpd_uri_t planned_route_uri = {
        .uri = "/api/route",
        .method = HTTP_GET,
        .handler = [](httpd_req_t* req) -> esp_err_t {
            PlannedRoute plannedRoute = FlightStorage.getPlannedRoute();
            auto pRPacket = PlannedRoutePacket{
                .route = plannedRoute,
            };
            nlohmann::json json = pRPacket;
            std::string jsonString = json.dump();
            return httpd_resp_send(req, jsonString.c_str(), jsonString.size());
        }
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &planned_route_uri));
}

// FreeRTOS task: periodically send websocket updates every second
void FrontendHandlerClass::sendWSTaskEntry(void* args) {
    FrontendHandlerClass* inst = FrontendHandlerClass::getInstancePtr();
    while (true) {
        inst->sendWSUpdate();
        vTaskDelay(pdMS_TO_TICKS(5000));  // Increased from 2000ms to 5000ms to reduce memory churn
    }
}
