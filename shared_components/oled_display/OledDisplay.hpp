#pragma once
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"
#include "types.hpp"

#include <array>
#include <cstdint>
#include <string>

class OledDisplayClass {
private:
    OledDisplayClass() = default;

public:
    ~OledDisplayClass() = delete;

    static OledDisplayClass* getInstancePtr();
    static OledDisplayClass& getInstance();

    void begin();

private:
    // Dedicated bus for the OLED - kept separate from I2CManager's shared bus
    // so the display can be wired to its own pins (see Kconfig) without
    // contending with the sensor bus.
    static constexpr i2c_port_num_t OLED_I2C_PORT = I2C_NUM_1;
    static constexpr uint32_t OLED_I2C_CLOCK_HZ = 400000;
    static constexpr uint8_t OLED_I2C_ADDR = 0x3C;
    static constexpr uint8_t OLED_WIDTH_PX = 128;
    static constexpr uint8_t OLED_HEIGHT_PX = 64;
    static constexpr size_t STATUS_LABEL_COUNT = 4;
    static constexpr uint32_t STATUS_TASK_INTERVAL_MS = 2000;

    bool initialized = false;
    i2c_master_bus_handle_t busHandle = nullptr;
    esp_lcd_panel_io_handle_t ioHandle = nullptr;
    esp_lcd_panel_handle_t panelHandle = nullptr;

    lv_display_t* lvglDisplay = nullptr;
    lv_obj_t* statusLabels[STATUS_LABEL_COUNT] = {};

    // LVGL's I1 (1bpp) draw buffer for the whole screen; +8 bytes for the
    // palette LVGL prepends to every I1 buffer.
    uint8_t lvglDrawBuffer[OLED_WIDTH_PX * OLED_HEIGHT_PX / 8 + 8] = {};
    // Scratch buffer holding the same frame converted to the SSD1315's
    // page-major (vtiled) GDDRAM layout, ready for esp_lcd_panel_draw_bitmap.
    uint8_t panelFrameBuffer[OLED_WIDTH_PX * OLED_HEIGHT_PX / 8] = {};

    void initLvgl();
    void createStatusLabels();
    void setStatusLines(const std::array<std::string, STATUS_LABEL_COUNT>& lines, size_t count);

    static void flushCallback(lv_display_t* disp, const lv_area_t* area, uint8_t* pxMap);
    static void roundInvalidatedAreaCallback(lv_event_t* e);
    static uint32_t tickCallback();

    static std::string connectionSummary(const ConnectionState* states, const char* const* names, size_t count);

    static void statusTaskEntry(void* param);
    [[noreturn]] void statusTask();
};

extern OledDisplayClass& OledDisplay;