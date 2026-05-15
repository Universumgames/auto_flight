#include "websocket_helper.hpp"

#include "Frontend.hpp"

static void ws_ping_send(void* arg) {
    auto* resp_arg = (async_resp_arg*)arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ping_pkt = {};
    ping_pkt.type = HTTPD_WS_TYPE_TEXT;
    ping_pkt.payload = (uint8_t*)"pong";
    ping_pkt.len = strlen("pong");
    httpd_ws_send_frame_async(hd, fd, &ping_pkt);
    free(resp_arg);
}

esp_err_t respondPing(httpd_handle_t handle, httpd_req_t* req) {
    auto* resp_arg = (async_resp_arg*)malloc(sizeof(struct async_resp_arg));
    if (resp_arg == nullptr) {
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

