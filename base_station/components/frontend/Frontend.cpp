#include "Frontend.hpp"

#include "esp_http_server.h"

#define ESP_ERROR_CHECK_SOFT(err) \
if(err != ESP_OK) { \
ESP_ERROR_CHECK_WITHOUT_ABORT(err); \
return err; \
}

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

FrontendHandlerClass::FrontendHandlerClass() {}

void FrontendHandlerClass::init() {
    config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    esp_err_t err;
    err = httpd_start(&httpd_handle, &config);
    ESP_ERROR_CHECK(err);

    mountFS();

    registerURIHandlers();
}


void FrontendHandlerClass::registerURIHandlers() {
    registerPing();
    registerWebsocket();
    registerStaticURIHandler();
}

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

void FrontendHandlerClass::registerPing() {
    static httpd_uri_t ping_uri = {
        .uri = "/ping",
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

static esp_err_t ws_handler(httpd_req_t *req) {
    return ESP_OK;
}

void FrontendHandlerClass::registerWebsocket() {
    static httpd_uri_t websocket_uri = {
        .uri = "/socket.io",
        .method = HTTP_GET,
        .handler = ws_handler,
        .is_websocket = true,
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &websocket_uri));
}

#pragma GCC diagnostic pop
