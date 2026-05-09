#pragma once
#include "esp_http_server.h"

class FrontendHandlerClass {
private:
    FrontendHandlerClass();
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

    void mountFS();
    void registerStaticURIHandler();
private:
    httpd_handle_t httpd_handle;
    httpd_config_t config;
};

extern FrontendHandlerClass& FrontendHandler;