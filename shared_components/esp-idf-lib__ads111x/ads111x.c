/*
 * Copyright (c) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * Copyright (c) 2020 Lucio Tarantino <https://github.com/dianlight>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of itscontributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file ads111x.c
 *
 * ESP-IDF driver for ADS1113/ADS1114/ADS1115, ADS1013/ADS1014/ADS1015 I2C ADC
 *
 * Ported from esp-open-rtos, rebased onto the espressif/i2c_bus component
 *
 * Copyright (c) 2016, 2018 Ruslan V. Uss <unclerus@gmail.com>
 * Copyright (c) 2020 Lucio Tarantino <https://github.com/dianlight>
 *
 * BSD Licensed as described in the file LICENSE
 */

#include <stdlib.h>
#include <esp_log.h>
#include "ads111x.h"

#define REG_CONVERSION 0
#define REG_CONFIG     1
#define REG_THRESH_L   2
#define REG_THRESH_H   3

#define COMP_QUE_OFFSET  0
#define COMP_QUE_MASK    0x03
#define COMP_LAT_OFFSET  2
#define COMP_LAT_MASK    0x01
#define COMP_POL_OFFSET  3
#define COMP_POL_MASK    0x01
#define COMP_MODE_OFFSET 4
#define COMP_MODE_MASK   0x01
#define DR_OFFSET        5
#define DR_MASK          0x07
#define MODE_OFFSET      8
#define MODE_MASK        0x01
#define PGA_OFFSET       9
#define PGA_MASK         0x07
#define MUX_OFFSET       12
#define MUX_MASK         0x07
#define OS_OFFSET        15
#define OS_MASK          0x01

#define CHECK(x) do { esp_err_t __; if ((__ = x) != ESP_OK) return __; } while (0)
#define CHECK_ARG(VAL) do { if (!(VAL)) return ESP_ERR_INVALID_ARG; } while (0)

static const char *TAG = "ads111x";

const float ads111x_gain_values[] =
{
    [ADS111X_GAIN_6V144]   = 6.144,
    [ADS111X_GAIN_4V096]   = 4.096,
    [ADS111X_GAIN_2V048]   = 2.048,
    [ADS111X_GAIN_1V024]   = 1.024,
    [ADS111X_GAIN_0V512]   = 0.512,
    [ADS111X_GAIN_0V256]   = 0.256,
    [ADS111X_GAIN_0V256_2] = 0.256,
    [ADS111X_GAIN_0V256_3] = 0.256
};

typedef struct {
    i2c_bus_device_handle_t i2c_dev;
    uint8_t addr;
} ads111x_dev_t;

static esp_err_t read_reg(ads111x_dev_t *dev, uint8_t reg, uint16_t *val)
{
    uint8_t buf[2];
    esp_err_t res = i2c_bus_read_bytes(dev->i2c_dev, reg, 2, buf);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not read from register 0x%02x", reg);
        return res;
    }
    *val = (buf[0] << 8) | buf[1];

    return ESP_OK;
}

static esp_err_t write_reg(ads111x_dev_t *dev, uint8_t reg, uint16_t val)
{
    uint8_t buf[2] = { val >> 8, val };
    esp_err_t res = i2c_bus_write_bytes(dev->i2c_dev, reg, 2, buf);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not write 0x%04x to register 0x%02x", val, reg);
        return res;
    }

    return ESP_OK;
}

static esp_err_t read_conf_bits(ads111x_dev_t *dev, uint8_t offs, uint16_t mask,
                                uint16_t *bits)
{
    CHECK_ARG(dev);

    uint16_t val;
    CHECK(read_reg(dev, REG_CONFIG, &val));

    ESP_LOGD(TAG, "Got config value: 0x%04x", val);

    *bits = (val >> offs) & mask;

    return ESP_OK;
}

static esp_err_t write_conf_bits(ads111x_dev_t *dev, uint16_t val, uint8_t offs,
                                 uint16_t mask)
{
    CHECK_ARG(dev);

    uint16_t old;
    CHECK(read_reg(dev, REG_CONFIG, &old));
    // Issue #593
    if (offs != OS_OFFSET || mask != OS_MASK)
        old &= ~(OS_MASK << OS_OFFSET);
    CHECK(write_reg(dev, REG_CONFIG, (old & ~(mask << offs)) | (val << offs)));

    return ESP_OK;
}

#define READ_CONFIG(OFFS, MASK, VAR) do { \
        CHECK_ARG(VAR); \
        uint16_t bits; \
        CHECK(read_conf_bits((ads111x_dev_t *)dev, OFFS, MASK, &bits)); \
        *VAR = bits; \
        return ESP_OK; \
    } while(0)


///////////////////////////////////////////////////////////////////////////////

ads111x_handle_t ads111x_create(i2c_bus_handle_t bus, uint8_t addr)
{
    if (addr != ADS111X_ADDR_GND && addr != ADS111X_ADDR_VCC
            && addr != ADS111X_ADDR_SDA && addr != ADS111X_ADDR_SCL)
    {
        ESP_LOGE(TAG, "Invalid I2C address");
        return NULL;
    }

    ads111x_dev_t *dev = (ads111x_dev_t *) calloc(1, sizeof(ads111x_dev_t));
    dev->i2c_dev = i2c_bus_device_create(bus, addr, i2c_bus_get_current_clk_speed(bus));
    dev->addr = addr;

    return (ads111x_handle_t) dev;
}

void ads111x_delete(ads111x_handle_t dev)
{
    ads111x_dev_t *d = (ads111x_dev_t *) dev;
    free(d);
}

esp_err_t ads111x_is_busy(ads111x_handle_t dev, bool *busy)
{
    CHECK_ARG(dev && busy);

    uint16_t r;
    CHECK(read_conf_bits((ads111x_dev_t *)dev, OS_OFFSET, OS_MASK, &r));
    *busy = !r;

    return ESP_OK;
}

esp_err_t ads111x_start_conversion(ads111x_handle_t dev)
{
    return write_conf_bits((ads111x_dev_t *)dev, 1, OS_OFFSET, OS_MASK);
}

esp_err_t ads111x_get_value(ads111x_handle_t dev, int16_t *value)
{
    CHECK_ARG(dev && value);

    return read_reg((ads111x_dev_t *)dev, REG_CONVERSION, (uint16_t *)value);
}

esp_err_t ads101x_get_value(ads111x_handle_t dev, int16_t *value)
{
    CHECK_ARG(dev && value);

    CHECK(read_reg((ads111x_dev_t *)dev, REG_CONVERSION, (uint16_t *)value));

    *value = *value >> 4;
    if (*value > 0x07FF)
    {
        // negative number - extend the sign to 16th bit
        *value |= 0xF000;
    }
    return ESP_OK;
}

esp_err_t ads111x_get_gain(ads111x_handle_t dev, ads111x_gain_t *gain)
{
    READ_CONFIG(PGA_OFFSET, PGA_MASK, gain);
}

esp_err_t ads111x_set_gain(ads111x_handle_t dev, ads111x_gain_t gain)
{
    return write_conf_bits((ads111x_dev_t *)dev, gain, PGA_OFFSET, PGA_MASK);
}

esp_err_t ads111x_get_input_mux(ads111x_handle_t dev, ads111x_mux_t *mux)
{
    READ_CONFIG(MUX_OFFSET, MUX_MASK, mux);
}

esp_err_t ads111x_set_input_mux(ads111x_handle_t dev, ads111x_mux_t mux)
{
    return write_conf_bits((ads111x_dev_t *)dev, mux, MUX_OFFSET, MUX_MASK);
}

esp_err_t ads111x_get_mode(ads111x_handle_t dev, ads111x_mode_t *mode)
{
    READ_CONFIG(MODE_OFFSET, MODE_MASK, mode);
}

esp_err_t ads111x_set_mode(ads111x_handle_t dev, ads111x_mode_t mode)
{
    return write_conf_bits((ads111x_dev_t *)dev, mode, MODE_OFFSET, MODE_MASK);
}

esp_err_t ads111x_get_data_rate(ads111x_handle_t dev, ads111x_data_rate_t *rate)
{
    READ_CONFIG(DR_OFFSET, DR_MASK, rate);
}

esp_err_t ads111x_set_data_rate(ads111x_handle_t dev, ads111x_data_rate_t rate)
{
    return write_conf_bits((ads111x_dev_t *)dev, rate, DR_OFFSET, DR_MASK);
}

esp_err_t ads111x_get_comp_mode(ads111x_handle_t dev, ads111x_comp_mode_t *mode)
{
    READ_CONFIG(COMP_MODE_OFFSET, COMP_MODE_MASK, mode);
}

esp_err_t ads111x_set_comp_mode(ads111x_handle_t dev, ads111x_comp_mode_t mode)
{
    return write_conf_bits((ads111x_dev_t *)dev, mode, COMP_MODE_OFFSET, COMP_MODE_MASK);
}

esp_err_t ads111x_get_comp_polarity(ads111x_handle_t dev, ads111x_comp_polarity_t *polarity)
{
    READ_CONFIG(COMP_POL_OFFSET, COMP_POL_MASK, polarity);
}

esp_err_t ads111x_set_comp_polarity(ads111x_handle_t dev, ads111x_comp_polarity_t polarity)
{
    return write_conf_bits((ads111x_dev_t *)dev, polarity, COMP_POL_OFFSET, COMP_POL_MASK);
}

esp_err_t ads111x_get_comp_latch(ads111x_handle_t dev, ads111x_comp_latch_t *latch)
{
    READ_CONFIG(COMP_LAT_OFFSET, COMP_LAT_MASK, latch);
}

esp_err_t ads111x_set_comp_latch(ads111x_handle_t dev, ads111x_comp_latch_t latch)
{
    return write_conf_bits((ads111x_dev_t *)dev, latch, COMP_LAT_OFFSET, COMP_LAT_MASK);
}

esp_err_t ads111x_get_comp_queue(ads111x_handle_t dev, ads111x_comp_queue_t *queue)
{
    READ_CONFIG(COMP_QUE_OFFSET, COMP_QUE_MASK, queue);
}

esp_err_t ads111x_set_comp_queue(ads111x_handle_t dev, ads111x_comp_queue_t queue)
{
    return write_conf_bits((ads111x_dev_t *)dev, queue, COMP_QUE_OFFSET, COMP_QUE_MASK);
}

esp_err_t ads111x_get_comp_low_thresh(ads111x_handle_t dev, int16_t *th)
{
    CHECK_ARG(dev && th);

    return read_reg((ads111x_dev_t *)dev, REG_THRESH_L, (uint16_t *)th);
}

esp_err_t ads111x_set_comp_low_thresh(ads111x_handle_t dev, int16_t th)
{
    CHECK_ARG(dev);

    return write_reg((ads111x_dev_t *)dev, REG_THRESH_L, th);
}

esp_err_t ads111x_get_comp_high_thresh(ads111x_handle_t dev, int16_t *th)
{
    CHECK_ARG(dev && th);

    return read_reg((ads111x_dev_t *)dev, REG_THRESH_H, (uint16_t *)th);
}

esp_err_t ads111x_set_comp_high_thresh(ads111x_handle_t dev, int16_t th)
{
    CHECK_ARG(dev);

    return write_reg((ads111x_dev_t *)dev, REG_THRESH_H, th);
}
