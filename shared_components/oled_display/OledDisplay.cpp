#include "OledDisplay.hpp"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_oled_ssd1315.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "FlightStorage.hpp"
#include "helper.hpp"

#include <algorithm>
#include <iterator>

#include "DeviceId.hpp"

static const char* TAG_OLED_DISPLAY = "OledDisplay";

static OledDisplayClass* oledDisplayInstance = nullptr;

OledDisplayClass& OledDisplay = OledDisplayClass::getInstance();

OledDisplayClass* OledDisplayClass::getInstancePtr() {
    if (!oledDisplayInstance) {
        oledDisplayInstance = new OledDisplayClass();
    }
    return oledDisplayInstance;
}

OledDisplayClass& OledDisplayClass::getInstance() {
    return *getInstancePtr();
}

void OledDisplayClass::begin() {
    if (initialized) return;

    ESP_LOGI(TAG_OLED_DISPLAY, "Initializing OLED display, SDA=%d SCL=%d RESET=%d VEXT=%d",
             CONFIG_OLED_PIN_SDA, CONFIG_OLED_PIN_SCL, CONFIG_OLED_PIN_RESET, CONFIG_OLED_PIN_VEXT);

    // On Heltec boards the OLED sits behind a Vext power switch that is off
    // by default; without enabling it the panel has no power and every I2C
    // transaction to it blocks forever (the IDF I2C driver waits without a
    // timeout). Drive it LOW to switch the rail on.
    if (CONFIG_OLED_PIN_VEXT >= 0) {
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

        const gpio_num_t vextPin = static_cast<gpio_num_t>(CONFIG_OLED_PIN_VEXT);
        gpio_config_t vextConfig = {
            .pin_bit_mask = 1ULL << vextPin,
            .mode = GPIO_MODE_OUTPUT,
        };
        ESP_ERROR_CHECK(gpio_config(&vextConfig));
        ESP_ERROR_CHECK(gpio_set_level(vextPin, 0));
        vTaskDelay(pdMS_TO_TICKS(10));
        ESP_LOGI(TAG_OLED_DISPLAY, "OLED Vext power enabled");
    }

    i2c_master_bus_config_t busConfig = {
        .i2c_port = OLED_I2C_PORT,
        .sda_io_num = (gpio_num_t)CONFIG_OLED_PIN_SDA,
        .scl_io_num = (gpio_num_t)CONFIG_OLED_PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = true,
        },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&busConfig, &busHandle));
    ESP_LOGI(TAG_OLED_DISPLAY, "OLED I2C bus created");

    esp_lcd_panel_io_i2c_config_t ioConfig = {
        .dev_addr = OLED_I2C_ADDR,
        .scl_speed_hz = OLED_I2C_CLOCK_HZ,
        .control_phase_bytes = 1,
        .dc_bit_offset = 6,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(busHandle, &ioConfig, &ioHandle));
    ESP_LOGI(TAG_OLED_DISPLAY, "OLED panel IO created");

    esp_lcd_panel_ssd1315_config_t vendorConfig = {
        .height = OLED_HEIGHT_PX,
    };
    esp_lcd_panel_dev_config_t panelConfig = {
        .bits_per_pixel = 1,
        .reset_gpio_num = CONFIG_OLED_PIN_RESET >= 0
                              ? static_cast<gpio_num_t>(CONFIG_OLED_PIN_RESET)
                              : GPIO_NUM_NC,
        .vendor_config = &vendorConfig,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1315(ioHandle, &panelConfig, &panelHandle));
    ESP_LOGI(TAG_OLED_DISPLAY, "OLED panel driver created");

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panelHandle));
    ESP_LOGI(TAG_OLED_DISPLAY, "OLED panel reset done");
    ESP_ERROR_CHECK(esp_lcd_panel_init(panelHandle));
    ESP_LOGI(TAG_OLED_DISPLAY, "OLED panel init done");

    // esp_lcd_new_panel_ssd1315() leaves the controller in horizontal
    // addressing mode (column auto-increments fastest, page after each row),
    // but lv_draw_sw_i1_convert_to_vtiled() packs our framebuffer the other
    // way round: all 8 page-bytes of column 0, then column 1, etc. Switch the
    // controller to vertical addressing mode (SSD1306/1315 command 0x20,
    // param 0x01) so its auto-increment order matches that byte stream;
    // otherwise whole 8px blocks land transposed on screen.
    {
        static constexpr uint8_t SSD1315_CMD_SET_MEMORY_ADDR_MODE = 0x20;
        static constexpr uint8_t SSD1315_ADDR_MODE_VERTICAL = 0x01;
        ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(ioHandle, SSD1315_CMD_SET_MEMORY_ADDR_MODE,
            &SSD1315_ADDR_MODE_VERTICAL, 1));
    }
    ESP_LOGI(TAG_OLED_DISPLAY, "OLED vertical addressing mode set");
#pragma GCC diagnostic pop

    // Not inverted: our I1 framebuffer already uses bit=1 for lit (text)
    // pixels and bit=0 for the (mostly background) off pixels: inverting at
    // the controller flips that, lighting the whole background instead.
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panelHandle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panelHandle, true));
    ESP_LOGI(TAG_OLED_DISPLAY, "OLED panel turned on");

    initLvgl();
    ESP_LOGI(TAG_OLED_DISPLAY, "LVGL display initialized");
    createStatusLabels();
    ESP_LOGI(TAG_OLED_DISPLAY, "OLED status labels created");

    initialized = true;

    // lv_timer_handler() here drives LVGL font rendering, the I1->vtiled
    // conversion, and an esp_lcd/I2C draw - that call chain needs more than
    // the 4096B this task overflowed on.
    xTaskCreate(statusTaskEntry, "OledDisplayStatusTask", 8192, this, tskIDLE_PRIORITY + 1, nullptr);

    ESP_LOGI(TAG_OLED_DISPLAY, "OLED display initialized");
}

void OledDisplayClass::initLvgl() {
    lv_init();
    lv_tick_set_cb(tickCallback);

    lvglDisplay = lv_display_create(OLED_WIDTH_PX, OLED_HEIGHT_PX);
    lv_display_set_user_data(lvglDisplay, this);
    lv_display_set_color_format(lvglDisplay, LV_COLOR_FORMAT_I1);
    lv_display_set_flush_cb(lvglDisplay, flushCallback);
    lv_display_set_buffers(lvglDisplay, lvglDrawBuffer, nullptr, sizeof(lvglDrawBuffer),
                           LV_DISPLAY_RENDER_MODE_FULL);
    // I1 buffers must be flushed in byte-aligned (8px) chunks; round every
    // invalidated area up so lv_draw_sw_i1_convert_to_vtiled's width/height
    // multiple-of-8 requirement always holds.
    lv_display_add_event_cb(lvglDisplay, roundInvalidatedAreaCallback, LV_EVENT_INVALIDATE_AREA, nullptr);

    lv_obj_t* screen = lv_display_get_screen_active(lvglDisplay);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
}

void OledDisplayClass::createStatusLabels() {
    lv_obj_t* screen = lv_display_get_screen_active(lvglDisplay);
    const uint8_t lineHeight = OLED_HEIGHT_PX / STATUS_LABEL_COUNT;

    for (size_t i = 0; i < STATUS_LABEL_COUNT; i++) {
        lv_obj_t* label = lv_label_create(screen);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        // Anti-aliased fonts barely register on a 1bpp display (LVGL's I1
        // blend only lights a pixel at 100% coverage) - unscii_8 is a crisp,
        // non-anti-aliased bitmap font made for exactly this.
        lv_obj_set_style_text_font(label, &lv_font_unscii_8, 0);
        lv_obj_set_pos(label, 0, static_cast<int32_t>(i * lineHeight));
        lv_obj_set_width(label, OLED_WIDTH_PX);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_CLIP);
        lv_label_set_text(label, "");
        statusLabels[i] = label;
    }
}

void OledDisplayClass::setStatusLines(const std::array<std::string, STATUS_LABEL_COUNT>& lines, size_t count) {
    for (size_t i = 0; i < STATUS_LABEL_COUNT; i++) {
        lv_label_set_text(statusLabels[i], i < count ? lines[i].c_str() : "");
    }
    lv_timer_handler();
}

void OledDisplayClass::flushCallback(lv_display_t* disp, const lv_area_t* area, uint8_t* pxMap) {
    auto* instance = static_cast<OledDisplayClass*>(lv_display_get_user_data(disp));

    // In LV_DISPLAY_RENDER_MODE_FULL the draw buffer's area is always the
    // whole screen, so px_map covers the full OLED_WIDTH_PX x OLED_HEIGHT_PX
    // image (not a sub-image cropped to `area`); flush it in full every time.
    (void)area;

    // LVGL prepends an 8-byte palette to every I1 buffer; skip it.
    pxMap += 8;

    lv_draw_sw_i1_convert_to_vtiled(pxMap, sizeof(instance->lvglDrawBuffer) - 8, OLED_WIDTH_PX, OLED_HEIGHT_PX,
                                    instance->panelFrameBuffer, sizeof(instance->panelFrameBuffer), true);

    esp_lcd_panel_draw_bitmap(instance->panelHandle, 0, 0, OLED_WIDTH_PX, OLED_HEIGHT_PX,
                              instance->panelFrameBuffer);

    lv_display_flush_ready(disp);
}

void OledDisplayClass::roundInvalidatedAreaCallback(lv_event_t* e) {
    auto* area = static_cast<lv_area_t*>(lv_event_get_param(e));
    area->x1 &= ~0x7;
    area->x2 |= 0x7;
    area->y1 &= ~0x7;
    area->y2 |= 0x7;
}

uint32_t OledDisplayClass::tickCallback() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

std::string OledDisplayClass::connectionSummary(const ConnectionState* states, const char* const* names,
                                                size_t count) {
    const auto connected = std::count(states, states + count, ConnectionState::CONNECTED);
    /*for (size_t i = 0; i < count; i++) {
        if (states[i] != ConnectionState::CONNECTED) {
            ESP_LOGW(TAG_OLED_DISPLAY, "%s not connected", names[i]);
        }
    }*/
    return std::to_string(connected) + "/" + std::to_string(count) +
        (static_cast<size_t>(connected) == count ? " OK" : " !!");
}

void OledDisplayClass::statusTaskEntry(void* param) {
    auto* instance = static_cast<OledDisplayClass*>(param);
    instance->statusTask();
}

[[noreturn]] void OledDisplayClass::statusTask() {
    while (true) {
        const bool isPlane = isDevicePlane();

        std::array<std::string, STATUS_LABEL_COUNT> lines;
        size_t lineCount = 0;
        lines[lineCount++] = std::string("Role: ") + (isPlane ? "PLANE" : "BASE");

#ifdef FLIGHT_DEVICE_TYPE_PLANE
        const ConnectionState linkState = FlightStorage.getBaseConnectionState();
        lines[lineCount++] = std::string("Link: ") +
            (linkState == ConnectionState::CONNECTED ? "CONNECTED" : "CONNECTING");
#else
        const auto& knownPlanes = FlightStorage.getAllPlanes();
        const auto connectionCount = std::ranges::count_if(knownPlanes,
                                                           [](const auto& plane) {
                                                               return plane.second.planeConnectionState ==
                                                                   ConnectionState::CONNECTED;
                                                           });
        lines[lineCount++] = std::string("Connection: ") + std::to_string(connectionCount) + "/" + std::to_string(
                knownPlanes.size()) +
            (connectionCount == knownPlanes.size() ? " OK" : " !!");
#endif


#ifdef FLIGHT_DEVICE_TYPE_PLANE
        const ConnectionState planeDevices[] = {
            FlightStorage.getPlaneGPSConnectionState(),
            FlightStorage.getPlaneBarometerConnectionState(),
            FlightStorage.getPlaneMotorControlConnectionState(),
            FlightStorage.getPlaneMagnetometerConnectionState(),
            FlightStorage.getPlaneAccelerometerConnectionState(),
            FlightStorage.getPlaneBatteryConnectionState(),
        };

        static constexpr const char* planeDeviceNames[] = {
            "GPS", "Barometer",
            "MotorControl", "Magnetometer", "Accelerometer", "Battery"
        };
        lines[lineCount++] =
            "Plane: " + connectionSummary(planeDevices, planeDeviceNames, std::size(planeDevices));
        lines[lineCount++] = "ID: " + std::to_string(DeviceId::get32());
#else
        const ConnectionState baseDevices[] = {
            FlightStorage.getBaseGPSConnectionState(),
            FlightStorage.getBaseBarometerConnectionState(),
            FlightStorage.getBaseBatteryConnectionState()
        };
        static constexpr const char* baseDeviceNames[] = {
            "Base GPS", "Base Barometer", "Base Battery"
        };
        lines[lineCount++] =
            "Base: " + connectionSummary(baseDevices, baseDeviceNames, std::size(baseDevices));
#endif

        setStatusLines(lines, lineCount);

        vTaskDelay(pdMS_TO_TICKS(STATUS_TASK_INTERVAL_MS));
    }
}
