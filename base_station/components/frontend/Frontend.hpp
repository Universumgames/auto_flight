#pragma once
#include <vector>

#include "esp_http_server.h"
#include "FrontendPackets.hpp"

class FrontendHandlerClass {
    using BaseUpdatePacket = Frontend::BaseUpdatePacket;
    using FlightUpdatePacket = Frontend::FlightUpdatePacket;
    using ConnectionUpdatePacket = Frontend::ConnectionUpdatePacket;
    using AreaDefinePacket = Frontend::AreaDefinePacket;
    using SensorPacket = Frontend::SensorPacket;
    using PlannedRoutePacket = Frontend::PlannedRoutePacket;

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
