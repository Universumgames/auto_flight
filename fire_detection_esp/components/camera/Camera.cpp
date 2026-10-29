//
// Created by Tom Arlt on 22.10.26.
//

#include "Camera.hpp"

#include "esp_log.h"
#include "soc/gpio_num.h"
#include "driver/gpio.h"


Camera::Camera() {
}

esp_err_t Camera::init() {
    //power up the camera if PWDN pin is defined
    if(CAM_PIN_PWDN != -1){
        gpio_set_direction((gpio_num_t)CAM_PIN_PWDN, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)CAM_PIN_PWDN, 0);
    }

    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }

    return ESP_OK;
}

esp_err_t Camera::captureFrame(const std::function<void(FrameData)>& callback) {
    // Capture a frame
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Camera capture failed");
        return ESP_ERR_INVALID_STATE;
    }

     processImage(fb, callback);

    esp_camera_fb_return(fb);
    // Return the frame buffer pointer
    return ESP_OK;
}

void Camera::processImage(const camera_fb_t* frame, const std::function<void(FrameData)>& callback) {
    FrameData frameData{
        .data = frame->buf,
        .length = frame->len,
        .width = frame->width,
        .height = frame->height,
        .format = frame->format
    };



    // Call the callback function with the processed image data
    callback(frameData);

}
