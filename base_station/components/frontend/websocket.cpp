#include <algorithm>

#include "esp_log.h"
#include "Frontend.hpp"
#include "websocket_helper.hpp"

const char* TAG_WEBSOCKET = "WEBSOCKET";

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"


static esp_err_t ws_handler(httpd_req_t* req) {
    httpd_ws_frame_t ws_pkt;
    uint8_t* buf = nullptr;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_WEBSOCKET, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    ESP_LOGD(TAG_WEBSOCKET, "frame len is %d", ws_pkt.len);
    if (ws_pkt.len) {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = (uint8_t*)calloc(1, ws_pkt.len + 1);
        if (buf == nullptr) {
            ESP_LOGE(TAG_WEBSOCKET, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG_WEBSOCKET, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        ESP_LOGD(TAG_WEBSOCKET, "Got packet with message: %s", ws_pkt.payload);
    }
    ESP_LOGD(TAG_WEBSOCKET, "Packet type: %d", ws_pkt.type);
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT &&
        ws_pkt.payload != nullptr) {
        if (strncmp((char*)ws_pkt.payload, "ping", strlen("ping")) == 0) {
            free(buf);
            return respondPing(req->handle, req);
        }
        ESP_LOGW(TAG_WEBSOCKET, "Unhandled packet: %s", ws_pkt.payload);
        free(buf);
    }

    ret = httpd_ws_send_frame(req, &ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_WEBSOCKET, "httpd_ws_send_frame failed with %d", ret);
    }
    free(buf);
    return ret;
}

static esp_err_t ws_post_handshake_cb(httpd_req_t* req) {
    ESP_LOGD(TAG_WEBSOCKET, "=== ws_post_handshake_cb called ===");

    // Get the URI with query string
    const char* uri = req->uri;
    ESP_LOGD(TAG_WEBSOCKET, "WebSocket connection established for URI: %s", uri ? uri : "NULL");

    // Store the server handle and socket fd. Do NOT store the httpd_req_t pointer
    // because it is only valid during the request handler.
    FrontendHandlerClass::getInstance().addClient(req->handle, httpd_req_to_sockfd(req));
    return ESP_OK;
}

esp_err_t FrontendHandlerClass::sendWSPacket(const WebsocketClient& client, httpd_ws_frame_t* ws_pkt) {
    esp_err_t ret = httpd_ws_send_frame_async(client.handle, client.socketFd, ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_WEBSOCKET, "httpd_ws_send_frame_async failed with %d", ret);
    }
    return ret;
}

/*
 * async send function, which we put into the httpd work queue
 */
static void ws_async_send(void* arg) {
    auto* resp_arg = (async_resp_arg*)arg;

    esp_err_t r = httpd_ws_send_frame_async(
        resp_arg->hd,
        resp_arg->fd,
        &resp_arg->ws_pkt
    );
    if (r != ESP_OK) {
        ESP_LOGE(TAG_WEBSOCKET, "httpd_ws_send_frame_async (worker) failed with %d (%s)", r, esp_err_to_name(r));
    }

    free(resp_arg->payload);
    // resp_arg was allocated with malloc; free it accordingly.
    free(resp_arg);
}

static esp_err_t trigger_async_send(httpd_handle_t handle, int fd, httpd_ws_frame_t* ws_pkt) {
    auto* resp_arg = (async_resp_arg*)malloc(sizeof(async_resp_arg));
    if (!resp_arg) return ESP_ERR_NO_MEM;

    resp_arg->hd = handle;
    resp_arg->fd = fd;
    resp_arg->ws_pkt = *ws_pkt;

    if (ws_pkt->len > 0 && ws_pkt->payload) {
        resp_arg->payload = (uint8_t*)malloc(ws_pkt->len);
        if (!resp_arg->payload) {
            free(resp_arg);
            return ESP_ERR_NO_MEM;
        }
        memcpy(resp_arg->payload, ws_pkt->payload, ws_pkt->len);
        resp_arg->ws_pkt.payload = resp_arg->payload;
    } else {
        resp_arg->payload = nullptr;
    }

    esp_err_t ret = httpd_queue_work(handle, ws_async_send, resp_arg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_WEBSOCKET, "httpd_queue_work failed with %d (%s)", ret, esp_err_to_name(ret));
        free(resp_arg->payload);
        free(resp_arg);
    }

    return ret;
}

esp_err_t FrontendHandlerClass::sendWSPacketAsync(const WebsocketClient& client, httpd_ws_frame_t* ws_pkt) {
    return trigger_async_send(client.handle, client.socketFd, ws_pkt);
}

void FrontendHandlerClass::registerWebsocket() {
    static httpd_uri_t websocket_uri = {
        .uri = "/api/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .is_websocket = true,
        .ws_post_handshake_cb = ws_post_handshake_cb,
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &websocket_uri));
}

void FrontendHandlerClass::broadcastWSPacket(httpd_ws_frame_t* ws_pkt) {
    std::vector<WebsocketClient> clientsToRemove;
    for (auto client : clients) {
        // Use the stored handle for this client when checking fd info
        if (httpd_ws_get_fd_info(client.handle, client.socketFd) != HTTPD_WS_CLIENT_WEBSOCKET) {
            clientsToRemove.push_back(client);
            continue;
        }
        esp_err_t ret = sendWSPacketAsync(client, ws_pkt);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG_WEBSOCKET, "Failed to send WebSocket packet to client with fd %d, with error %d (%s)", client.socketFd, ret, esp_err_to_name(ret));

            clientsToRemove.push_back(client);
        }
    }
    for (const auto& client : clientsToRemove) {
        removeClient(client);
    }
}

void FrontendHandlerClass::removeClient(const WebsocketClient client) {
    std::erase(clients, client);
}

#pragma GCC diagnostic pop
