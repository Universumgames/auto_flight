#include "esp_littlefs.h"
#include "esp_log.h"
#include "Frontend.hpp"

const char* TAG_FRONTEND_FS = "frontend_fs";

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

void init_filesystem()
{
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/static",
        .partition_label = "storage",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG_FRONTEND_FS, "Failed to mount LittleFS (%s)", esp_err_to_name(ret));
        return;
    }

    size_t total = 0;
    size_t used = 0;

    ret = esp_littlefs_info("storage", &total, &used);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG_FRONTEND_FS, "LittleFS: total=%d used=%d", total, used);
    }
}

void FrontendHandlerClass::mountFS() {
    init_filesystem();
}


esp_err_t send_file(httpd_req_t *req, const char *filepath)
{
    FILE *file = fopen(filepath, "rb");

    if (!file)
    {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    char chunk[1024];
    size_t read_bytes;

    while ((read_bytes = fread(chunk, 1, sizeof(chunk), file)) > 0)
    {
        if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK)
        {
            fclose(file);
            httpd_resp_sendstr_chunk(req, NULL);
            return ESP_FAIL;
        }
    }

    fclose(file);

    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

void set_content_type(httpd_req_t *req, const char *filepath)
{
    if (strstr(filepath, ".html"))
        httpd_resp_set_type(req, "text/html");
    else if (strstr(filepath, ".css"))
        httpd_resp_set_type(req, "text/css");
    else if (strstr(filepath, ".js"))
        httpd_resp_set_type(req, "application/javascript");
    else if (strstr(filepath, ".png"))
        httpd_resp_set_type(req, "image/png");
    else if (strstr(filepath, ".jpg"))
        httpd_resp_set_type(req, "image/jpeg");
    else
        httpd_resp_set_type(req, "text/plain");
}

esp_err_t static_get_handler(httpd_req_t *req)
{
    char filepath[256];

    ESP_LOGI(TAG_FRONTEND_FS, "Received request for URI: %s", req->uri);
    if (strcmp(req->uri, "/") == 0)
    {
        snprintf(filepath, sizeof(filepath),
                 "/static/index.html");
    }
    else
    {
        int written = snprintf(filepath,
                       sizeof(filepath),
                       "/static%.*s",
                       (int)(sizeof(filepath) - strlen("/static") - 1),
                       req->uri);

        if (written < 0 || written >= sizeof(filepath))
        {
            httpd_resp_send_err(req,
                                HTTPD_500_INTERNAL_SERVER_ERROR,
                                "Path too long");
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG_FRONTEND_FS, "Serving file: %s", filepath);

    set_content_type(req, filepath);

    return send_file(req, filepath);
}

void FrontendHandlerClass::registerStaticURIHandler() {
    static httpd_uri_t static_files = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = static_get_handler,
        .user_ctx = nullptr
    };

    httpd_register_uri_handler(httpd_handle, &static_files);
}

#pragma GCC diagnostic pop
