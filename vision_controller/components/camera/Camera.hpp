#pragma once
#include "esp_camera.h"
#include <cstdint>
#include <functional>
#include <stdexcept>

//WROVER-KIT PIN Map
#define CAM_PIN_PWDN    -1 //power down is not used
#define CAM_PIN_RESET   -1 //software reset will be performed
#define CAM_PIN_XCLK    21
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27

#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      19
#define CAM_PIN_D2      18
#define CAM_PIN_D1       5
#define CAM_PIN_D0       4
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

class Camera {
public:
    Camera();
    ~Camera() = delete;
    Camera(const Camera&) = delete;

    struct FrameData {
        uint8_t* data;
        size_t length;
        size_t width;
        size_t height;
        pixformat_t format;

        struct Pixel {
            uint8_t r;
            uint8_t g;
            uint8_t b;
        };
        /// Bytes per pixel for raw formats, 0 for compressed/unsupported ones (e.g. JPEG)
        [[nodiscard]] size_t bytesPerPixel() const {
            switch (format) {
                case PIXFORMAT_GRAYSCALE: return 1;
                case PIXFORMAT_RGB565:
                case PIXFORMAT_YUV422:    return 2;
                case PIXFORMAT_RGB888:    return 3;
                default:                  return 0;
            }
        }

        /// Bounds- and format-checked access to the pixel at column x, row y, converted to RGB888.
        /// Throws std::out_of_range for invalid coordinates and std::logic_error for formats
        /// without random access (JPEG).
        [[nodiscard]] Pixel at(size_t x, size_t y) const {
            const size_t bpp = bytesPerPixel();
            if (bpp == 0) {
                throw std::logic_error("FrameData::at: pixel format does not support random access");
            }
            if (x >= width || y >= height || (y * width + x + 1) * bpp > length) {
                throw std::out_of_range("FrameData::at: pixel coordinate out of range");
            }
            return (*this)[x, y];
        }

        /// Unchecked access to the pixel at column x, row y, converted to RGB888.
        /// Use at() if the coordinates/format are not guaranteed to be valid.
        [[nodiscard]] Pixel operator[](size_t x, size_t y) const {
            const size_t offset = (y * width + x) * bytesPerPixel();
            const uint8_t* p = data + offset;

            switch (format) {
                case PIXFORMAT_GRAYSCALE:
                    return {p[0], p[0], p[0]};
                case PIXFORMAT_RGB565: {
                    // esp32-camera stores RGB565 big-endian (high byte first)
                    const uint16_t c = static_cast<uint16_t>(p[0] << 8 | p[1]);
                    const auto r5 = static_cast<uint8_t>((c >> 11) & 0x1F);
                    const auto g6 = static_cast<uint8_t>((c >> 5) & 0x3F);
                    const auto b5 = static_cast<uint8_t>(c & 0x1F);
                    // scale to 8 bit, replicating high bits into the low bits
                    return {
                        static_cast<uint8_t>(r5 << 3 | r5 >> 2),
                        static_cast<uint8_t>(g6 << 2 | g6 >> 4),
                        static_cast<uint8_t>(b5 << 3 | b5 >> 2)
                    };
                }
                case PIXFORMAT_RGB888:
                    // esp32-camera uses BGR byte order for RGB888 buffers
                    return {p[2], p[1], p[0]};
                case PIXFORMAT_YUV422: {
                    // YUYV: each pixel pair shares U and V -> [Y0 U Y1 V]
                    const uint8_t* pair = data + (offset & ~static_cast<size_t>(3));
                    return yuvToRgb(p[0], pair[1], pair[3]);
                }
                default:
                    return {0, 0, 0};
            }
        }

    private:
        static uint8_t clamp(int v) {
            return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
        }

        // BT.601 integer conversion
        static Pixel yuvToRgb(uint8_t y, uint8_t u, uint8_t v) {
            const int c = y - 16;
            const int d = u - 128;
            const int e = v - 128;
            return {
                clamp((298 * c + 409 * e + 128) >> 8),
                clamp((298 * c - 100 * d - 208 * e + 128) >> 8),
                clamp((298 * c + 516 * d + 128) >> 8)
            };
        }
    };
private:
    constexpr static const char* TAG = "Camera";
    camera_config_t camera_config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,

        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,

        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG
        .frame_size = FRAMESIZE_UXGA,
        //QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates.

        .jpeg_quality = 12, //0-63, for OV series camera sensors, lower number means higher quality
        .fb_count = 1, //When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY, //CAMERA_GRAB_LATEST. Sets when buffers should be filled
        .jpeg_buffer_size = 0 //Set to a custom size in bytes for JPEG mode, or 0 to use the default size
    };
public:
    esp_err_t init();

    esp_err_t captureFrame(const std::function<void(FrameData)>& callback);

private:
    void processImage(const camera_fb_t* frame, const std::function<void(FrameData)>& callback);
};
