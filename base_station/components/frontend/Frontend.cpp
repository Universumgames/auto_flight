#include "Frontend.hpp"

#include "esp_http_server.h"
#include "FlightStorage.hpp"
#include "GPS_Reader.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ESP_ERROR_CHECK_SOFT(err) \
if(err != ESP_OK) { \
ESP_ERROR_CHECK_WITHOUT_ABORT(err); \
return err; \
}

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"


static FrontendHandlerClass* instance = nullptr;

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

// forward declaration of the websocket update task function
static void ws_update_task(void* pvParameters);

void FrontendHandlerClass::init() {
    config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    esp_err_t err;
    err = httpd_start(&httpd_handle, &config);
    ESP_ERROR_CHECK(err);

    mountFS();

    registerURIHandlers();
    // start a FreeRTOS task to send websocket updates every second
    BaseType_t xReturned = xTaskCreate(
        ws_update_task,
        "ws_update_task",
        4096,
        nullptr,
        tskIDLE_PRIORITY + 1,
        nullptr
    );
    (void)xReturned;
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
    static httpd_uri_t ping_uri = {
        .uri = "/api/ping",
        .method = HTTP_GET,
        .handler = [](httpd_req_t* req) -> esp_err_t {
            const char* resp_str = "pong";
            httpd_resp_send(req, resp_str, strlen(resp_str));
            return ESP_OK;
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
        .planePosition = FlightStorage.getLastPlanePosition(),
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
    httpd_ws_frame_t* flightPkt = prepareFlightPacket();
    if (flightPkt != nullptr) {
        broadcastWSPacket(flightPkt);
        // free the payload we allocated in prepareFlightPacket
        free(flightPkt->payload);
        delete flightPkt;
    }
    httpd_ws_frame_t* connectionPkt = prepareConnectionPacket();
    if (connectionPkt != nullptr) {
        broadcastWSPacket(connectionPkt);
        free(connectionPkt->payload);
        delete connectionPkt;
    }
}

httpd_ws_frame_t* FrontendHandlerClass::prepareConnectionPacket() {
    ConnectionUpdatePacket rawPacket = {
        .baseConnectionState = ConnectionState::CONNECTED,
        .lastContactBaseStationTimestamp = GPS_Reader.getGPSLatestTime(),
        .planeConnectionState = FlightStorage.getPlaneConnectionState(),
        .lastContactPlaneTimestamp = FlightStorage.getLastConnectionTimestampPlane(),
        .gpsConnectionBase = GPS_Reader.hasValidPosition() ? ConnectionState::CONNECTED : ConnectionState::CONNECTING,
        .gpsConnectionPlane = GPS_Reader.getGPSLatestTime() - FlightStorage.getLastConnectionTimestampPlane() < 10 ? ConnectionState::CONNECTED : ConnectionState::CONNECTING,
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
    static httpd_uri_t status_uri = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = [](httpd_req_t* req) -> esp_err_t {
            ConnectionUpdatePacket rawPacket = {
                .baseConnectionState = ConnectionState::CONNECTED,
                .lastContactBaseStationTimestamp = GPS_Reader.getGPSLatestTime(),
                .planeConnectionState = FlightStorage.getPlaneConnectionState(),
                .lastContactPlaneTimestamp = FlightStorage.getLastConnectionTimestampPlane(),
                .gpsConnectionBase = GPS_Reader.hasValidPosition() ? ConnectionState::CONNECTED : ConnectionState::CONNECTING,
                .gpsConnectionPlane = GPS_Reader.getGPSLatestTime() - FlightStorage.getLastConnectionTimestampPlane() < 10 ? ConnectionState::CONNECTED : ConnectionState::CONNECTING,
            };
            nlohmann::json json = rawPacket;
            std::string jsonString = json.dump();

            return httpd_resp_send(req, jsonString.c_str(), jsonString.size() + 1);
        },
        .user_ctx = nullptr,
        .is_websocket = false,
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &status_uri));


}

// FreeRTOS task: periodically send websocket updates
static void ws_update_task(void* pvParameters) {
    FrontendHandlerClass* inst = FrontendHandlerClass::getInstancePtr();
    for (;;) {
        inst->sendWSUpdate();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
