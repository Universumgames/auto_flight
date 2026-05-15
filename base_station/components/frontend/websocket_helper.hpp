#pragma once
#include "esp_http_server.h"

/*
 * Structure holding server handle
 * and internal socket fd in order
 * to use out of request send
 */
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
    httpd_ws_frame_t ws_pkt;
    uint8_t* payload;
};

esp_err_t respondPing(httpd_handle_t handle, httpd_req_t* req);