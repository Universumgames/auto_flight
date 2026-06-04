#pragma once
#include <vector>

#include "esp_http_server.h"
#include "types.hpp"

struct BaseUpdatePacket {
    static constexpr const char* type = "base";

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(BaseUpdatePacket, type)
};

struct FlightUpdatePacket {
    static constexpr const char* type = "flight";
    Coordinate basePosition;
    time_t basePositionUpdateTime;
    Coordinate planePosition;
    time_t planePositionUpdateTime;
    FlightRoute flightRoute;
    time_t flightRouteUpdateTime;
    PlannedRoute plannedRoute;
    time_t plannedRouteUpdateTime;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(FlightUpdatePacket, type, basePosition, basePositionUpdateTime, planePosition,
                                   planePositionUpdateTime, flightRoute, flightRouteUpdateTime, plannedRoute,
                                   plannedRouteUpdateTime)
};

struct ConnectionUpdatePacket{
    static constexpr const char* type = "connection";
    ConnectionState baseConnectionState;
    time_t lastContactBaseStationTimestamp;
    ConnectionState planeConnectionState;
    time_t lastContactPlaneTimestamp;

    ConnectionState gpsConnectionBase;
    ConnectionState gpsConnectionPlane;

    ConnectionState barometerConnectionBase;
    ConnectionState barometerConnectionPlane;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ConnectionUpdatePacket, type, baseConnectionState, lastContactBaseStationTimestamp,
                                   planeConnectionState, lastContactPlaneTimestamp, gpsConnectionBase,
                                   gpsConnectionPlane, barometerConnectionBase, barometerConnectionPlane)
};

struct AreaDefinePacket {
    std::vector<Coordinate> shape;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(AreaDefinePacket, shape)
};

struct SensorPacket {
    static constexpr const char* type = "sensor";
    float barometerPressureBase;
    float barometerPressurePlane;
    float calculatedAltitude;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SensorPacket, type, barometerPressureBase, barometerPressurePlane, calculatedAltitude)
};

class FrontendHandlerClass {
private:
    FrontendHandlerClass();
    static const char* TAG_FRONTEND;

public:
    ~FrontendHandlerClass() = delete;

    static FrontendHandlerClass* getInstancePtr();
    static FrontendHandlerClass& getInstance();

public:
    void init();

private:
    void registerURIHandlers();
    void registerPing();
    void registerWebsocket();
    void registerAPISockets();

    void mountFS();
    void registerStaticURIHandler();

public:
    struct WebsocketClient {
        // Do NOT store a pointer to httpd_req_t across the lifetime of the connection.
        // httpd_req_t is valid only during the request handler invocation. Store
        // the server handle and socket fd so we can send outside the request context.
        httpd_handle_t handle;
        int socketFd;

        bool operator==(const WebsocketClient& client) const {
            return socketFd == client.socketFd;
        }
    };

    [[nodiscard]] std::vector<WebsocketClient> getClients() const {
        return clients;
    }

    WebsocketClient addClient(httpd_handle_t handle, int socketFd) {
        const WebsocketClient client = {handle, socketFd};
        clients.push_back(client);
        return client;
    }

private:
    std::vector<WebsocketClient> clients;

    void removeClient(WebsocketClient client);

public:
    void broadcastWSPacket(httpd_ws_frame_t* ws_pkt);
    static esp_err_t sendWSPacket(const WebsocketClient& client, httpd_ws_frame_t* ws_pkt);
    static esp_err_t sendWSPacketAsync(const WebsocketClient& client, httpd_ws_frame_t* ws_pkt);

    void sendWSUpdate();

    [[noreturn]] static void sendWSTaskEntry(void* param);

private:
    /**
     * Creates a new websocket frame containing the current flight update data. The caller is responsible for freeing the returned frame after use.
     * @return websocket frame
     */
    httpd_ws_frame_t* prepareFlightPacket();

    httpd_ws_frame_t* prepareConnectionPacket();

    httpd_ws_frame_t* prepareSensorPacket();

private:
    httpd_handle_t httpd_handle = nullptr;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
};

extern FrontendHandlerClass& FrontendHandler;
