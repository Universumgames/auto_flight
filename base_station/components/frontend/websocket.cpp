#include "esp_log.h"
#include "Frontend.hpp"

const char* TAG_WEBSOCKET = "WEBSOCKET";

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

/*
 * Structure holding server handle
 * and internal socket fd in order
 * to use out of request send
 */
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

static esp_err_t ws_handler(httpd_req_t* req) {
    return ESP_OK;
}

/*
 * async send function, which we put into the httpd work queue
 */
static void ws_async_send(void* arg) {
    static const char* data = "Async data";
    async_resp_arg* resp_arg = (async_resp_arg*)arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)data;
    ws_pkt.len = strlen(data);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg);
}

static void ws_ping_send(void* arg) {
    struct async_resp_arg* resp_arg = (async_resp_arg*)arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ping_pkt;
    memset(&ping_pkt, 0, sizeof(httpd_ws_frame_t));
    ping_pkt.type = HTTPD_WS_TYPE_PING;
    httpd_ws_send_frame_async(hd, fd, &ping_pkt);
    free(resp_arg);
}

static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t* req) {
    struct async_resp_arg* resp_arg = (async_resp_arg*)malloc(sizeof(struct async_resp_arg));
    if (resp_arg == NULL) {
        return ESP_ERR_NO_MEM;
    }
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    esp_err_t ret = httpd_queue_work(handle, ws_async_send, resp_arg);
    if (ret != ESP_OK) {
        free(resp_arg);
    }
    return ret;
}


static esp_err_t trigger_ping_send(httpd_handle_t handle, httpd_req_t* req) {
    async_resp_arg* resp_arg = (async_resp_arg*)malloc(sizeof(struct async_resp_arg));
    if (resp_arg == NULL) {
        return ESP_ERR_NO_MEM;
    }
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    esp_err_t ret = httpd_queue_work(handle, ws_ping_send, resp_arg);
    if (ret != ESP_OK) {
        free(resp_arg);
    }
    return ret;
}

static esp_err_t echo_handler(httpd_req_t* req) {
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
    ESP_LOGI(TAG_WEBSOCKET, "frame len is %d", ws_pkt.len);
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
        ESP_LOGI(TAG_WEBSOCKET, "Got packet with message: %s", ws_pkt.payload);
    }
    ESP_LOGI(TAG_WEBSOCKET, "Packet type: %d", ws_pkt.type);
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT &&
        ws_pkt.payload != NULL) {
        if (strncmp((char*)ws_pkt.payload, "Trigger async", strlen("Trigger async")) == 0) {
            free(buf);
            return trigger_async_send(req->handle, req);
        }
        else if (strncmp((char*)ws_pkt.payload, "Ping", strlen("Ping")) == 0) {
            free(buf);
            return trigger_ping_send(req->handle, req);
        }
    }

    ret = httpd_ws_send_frame(req, &ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_WEBSOCKET, "httpd_ws_send_frame failed with %d", ret);
    }
    free(buf);
    return ret;
}

static esp_err_t ws_post_handshake_cb(httpd_req_t* req) {
    ESP_LOGI(TAG_WEBSOCKET, "=== ws_post_handshake_cb called ===");

    // Get the URI with query string
    const char* uri = req->uri;
    ESP_LOGI(TAG_WEBSOCKET, "WebSocket connection established for URI: %s", uri ? uri : "NULL");

    // Send a welcome message to the client
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.payload = (uint8_t*)"Welcome to the WebSocket Echo Server (post-handshake)!";
    ws_pkt.len = strlen((char*)ws_pkt.payload);
    esp_err_t ret = httpd_ws_send_frame(req, &ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_WEBSOCKET, "httpd_ws_send_frame failed with %d", ret);
        return ret;
    }
    return ESP_OK;
}

void FrontendHandlerClass::registerWebsocket() {
    static httpd_uri_t websocket_uri = {
        .uri = "/api/ws",
        .method = HTTP_GET,
        .handler = echo_handler,
        .is_websocket = true,
        .ws_post_handshake_cb = ws_post_handshake_cb,
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &websocket_uri));
}


#pragma GCC diagnostic pop
