/**
 * @file qmc5883p_eil.c
 *
 * ESP-IDF driver for the 3-axis digital compass QMC5883P, API modeled after
 * esp-idf-lib/hmc5883l by Ruslan V. Uss.
 */
#include "qmc5883p_eil.h"
#include <inttypes.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <i2c_bus.h>

#define REG_CHIP_ID 0x00
#define REG_DX_L    0x01
#define REG_DX_H    0x02
#define REG_DY_L    0x03
#define REG_DY_H    0x04
#define REG_DZ_L    0x05
#define REG_DZ_H    0x06
#define REG_STAT    0x09
#define REG_CTRL1   0x0a
#define REG_CTRL2   0x0b
#define REG_SIGN    0x29

// CTRL1
#define BIT_MODE 0
#define BIT_ODR  2
#define BIT_OSR1 4
#define BIT_OSR2 6

#define MASK_MODE 0x03
#define MASK_ODR  0x0c
#define MASK_OSR1 0x30
#define MASK_OSR2 0xc0

// CTRL2
#define BIT_SET_RESET 0
#define BIT_RNG       2

#define MASK_SET_RESET 0x03
#define MASK_RNG       0x0c
#define MASK_SELF_TEST 0x40
#define MASK_SOFT_RST  0x80

// STATUS
#define MASK_DRDY 0x01
#define MASK_OVFL 0x02

// Axis sign configuration from the datasheet setup examples
#define SIGN_DEFAULT 0x06

#define SOFT_RESET_DELAY_MS 50

static const char *TAG = "qmc5883p_eil";

static const float gain_values[] =
{
    [QMC5883P_EIL_GAIN_1000]  = 1000.0f / 1000.0f,
    [QMC5883P_EIL_GAIN_2500]  = 1000.0f / 2500.0f,
    [QMC5883P_EIL_GAIN_3750]  = 1000.0f / 3750.0f,
    [QMC5883P_EIL_GAIN_15000] = 1000.0f / 15000.0f
};

#define timeout_expired(start, len) ((uint64_t)(esp_timer_get_time() - (start)) >= (len))
#define CHECK_ARG(VAL) do { if (!(VAL)) return ESP_ERR_INVALID_ARG; } while (0)
#define CHECK(X) do { \
        esp_err_t __ = X; \
        if (__ != ESP_OK) return __; \
    } while (0)

static esp_err_t write_register(qmc5883p_eil_dev_t *dev, uint8_t reg, uint8_t val)
{
    esp_err_t ret = i2c_bus_write_byte(dev->dev_handle, reg, val);
    if (ret != ESP_OK)
        ESP_LOGE(TAG, "Could not write 0x%02x to register 0x%02x, err = %d", val, reg, ret);
    return ret;
}

static esp_err_t read_register(qmc5883p_eil_dev_t *dev, uint8_t reg, uint8_t *val)
{
    esp_err_t ret = i2c_bus_read_byte(dev->dev_handle, reg, val);
    if (ret != ESP_OK)
        ESP_LOGE(TAG, "Could not read register 0x%02x, err = %d", reg, ret);
    return ret;
}

static esp_err_t update_register(qmc5883p_eil_dev_t *dev, uint8_t reg, uint8_t mask, uint8_t val)
{
    uint8_t old;
    CHECK(read_register(dev, reg, &old));
    return write_register(dev, reg, (old & ~mask) | (val & mask));
}

static esp_err_t read_field(qmc5883p_eil_dev_t *dev, uint8_t reg, uint8_t mask, uint8_t bit, uint8_t *val)
{
    uint8_t raw;
    CHECK(read_register(dev, reg, &raw));
    *val = (raw & mask) >> bit;
    return ESP_OK;
}

esp_err_t qmc5883p_eil_init_desc(qmc5883p_eil_dev_t *dev, i2c_bus_handle_t bus_handle)
{
    CHECK_ARG(dev && bus_handle);

    dev->bus_handle = bus_handle;
    dev->dev_handle = i2c_bus_device_create(bus_handle, QMC5883P_EIL_ADDR, i2c_bus_get_current_clk_speed(bus_handle));
    if (dev->dev_handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to create I2C device");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t qmc5883p_eil_free_desc(qmc5883p_eil_dev_t *dev)
{
    CHECK_ARG(dev);

    if (dev->dev_handle)
    {
        i2c_bus_device_delete(&dev->dev_handle);
        dev->dev_handle = NULL;
    }
    return ESP_OK;
}

esp_err_t qmc5883p_eil_soft_reset(qmc5883p_eil_dev_t *dev)
{
    CHECK_ARG(dev);

    CHECK(write_register(dev, REG_CTRL2, MASK_SOFT_RST));
    vTaskDelay(pdMS_TO_TICKS(SOFT_RESET_DELAY_MS));
    return ESP_OK;
}

esp_err_t qmc5883p_eil_init(qmc5883p_eil_dev_t *dev)
{
    CHECK_ARG(dev);

    uint8_t id = 0;
    esp_err_t ret = i2c_bus_read_byte(dev->dev_handle, REG_CHIP_ID, &id);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not read chip ID, err = %d", ret);
        return ret;
    }
    if (id != QMC5883P_EIL_ID)
    {
        ESP_LOGE(TAG, "Unknown ID: 0x%02x != 0x%02x", id, QMC5883P_EIL_ID);
        return ESP_ERR_NOT_FOUND;
    }

    // Start from a known register state, this also recovers a sensor that
    // got stuck in some odd configuration when init is called again.
    CHECK(qmc5883p_eil_soft_reset(dev));
    CHECK(write_register(dev, REG_SIGN, SIGN_DEFAULT));

    qmc5883p_eil_gain_t gain;
    CHECK(qmc5883p_eil_get_gain(dev, &gain));
    dev->gain = gain_values[gain];

    CHECK(qmc5883p_eil_get_opmode(dev, &dev->opmode));

    return ESP_OK;
}

esp_err_t qmc5883p_eil_get_opmode(qmc5883p_eil_dev_t *dev, qmc5883p_eil_opmode_t *val)
{
    CHECK_ARG(dev && val);

    uint8_t raw;
    CHECK(read_field(dev, REG_CTRL1, MASK_MODE, BIT_MODE, &raw));
    *val = (qmc5883p_eil_opmode_t)raw;
    return ESP_OK;
}

esp_err_t qmc5883p_eil_set_opmode(qmc5883p_eil_dev_t *dev, qmc5883p_eil_opmode_t mode)
{
    CHECK_ARG(dev && mode <= QMC5883P_EIL_MODE_CONTINUOUS);

    uint8_t ctrl1;
    CHECK(read_register(dev, REG_CTRL1, &ctrl1));

    // Mode changes have to pass through suspend mode (datasheet 9.2.3). Doing
    // this for re-writes of the same mode as well makes re-arming single mode
    // reliable, regardless of whether the chip already cleared the mode bits.
    if ((ctrl1 & MASK_MODE) != (QMC5883P_EIL_MODE_SUSPEND << BIT_MODE) && mode != QMC5883P_EIL_MODE_SUSPEND)
        CHECK(write_register(dev, REG_CTRL1, (ctrl1 & ~MASK_MODE) | (QMC5883P_EIL_MODE_SUSPEND << BIT_MODE)));

    CHECK(write_register(dev, REG_CTRL1, (ctrl1 & ~MASK_MODE) | (mode << BIT_MODE)));
    dev->opmode = mode;
    return ESP_OK;
}

esp_err_t qmc5883p_eil_get_samples_averaged(qmc5883p_eil_dev_t *dev, qmc5883p_eil_samples_averaged_t *val)
{
    CHECK_ARG(dev && val);

    uint8_t raw;
    CHECK(read_field(dev, REG_CTRL1, MASK_OSR2, BIT_OSR2, &raw));
    *val = (qmc5883p_eil_samples_averaged_t)raw;
    return ESP_OK;
}

esp_err_t qmc5883p_eil_set_samples_averaged(qmc5883p_eil_dev_t *dev, qmc5883p_eil_samples_averaged_t samples)
{
    CHECK_ARG(dev && samples <= QMC5883P_EIL_SAMPLES_8);

    return update_register(dev, REG_CTRL1, MASK_OSR2, samples << BIT_OSR2);
}

esp_err_t qmc5883p_eil_get_oversampling(qmc5883p_eil_dev_t *dev, qmc5883p_eil_oversampling_t *val)
{
    CHECK_ARG(dev && val);

    uint8_t raw;
    CHECK(read_field(dev, REG_CTRL1, MASK_OSR1, BIT_OSR1, &raw));
    *val = (qmc5883p_eil_oversampling_t)raw;
    return ESP_OK;
}

esp_err_t qmc5883p_eil_set_oversampling(qmc5883p_eil_dev_t *dev, qmc5883p_eil_oversampling_t osr)
{
    CHECK_ARG(dev && osr <= QMC5883P_EIL_OSR_1);

    return update_register(dev, REG_CTRL1, MASK_OSR1, osr << BIT_OSR1);
}

esp_err_t qmc5883p_eil_get_data_rate(qmc5883p_eil_dev_t *dev, qmc5883p_eil_data_rate_t *val)
{
    CHECK_ARG(dev && val);

    uint8_t raw;
    CHECK(read_field(dev, REG_CTRL1, MASK_ODR, BIT_ODR, &raw));
    *val = (qmc5883p_eil_data_rate_t)raw;
    return ESP_OK;
}

esp_err_t qmc5883p_eil_set_data_rate(qmc5883p_eil_dev_t *dev, qmc5883p_eil_data_rate_t rate)
{
    CHECK_ARG(dev && rate <= QMC5883P_EIL_DATA_RATE_200);

    return update_register(dev, REG_CTRL1, MASK_ODR, rate << BIT_ODR);
}

esp_err_t qmc5883p_eil_get_set_reset(qmc5883p_eil_dev_t *dev, qmc5883p_eil_set_reset_t *val)
{
    CHECK_ARG(dev && val);

    uint8_t raw;
    CHECK(read_field(dev, REG_CTRL2, MASK_SET_RESET, BIT_SET_RESET, &raw));
    // 0b10 and 0b11 both mean set and reset off
    *val = raw >= QMC5883P_EIL_SET_RESET_OFF ? QMC5883P_EIL_SET_RESET_OFF : (qmc5883p_eil_set_reset_t)raw;
    return ESP_OK;
}

esp_err_t qmc5883p_eil_set_set_reset(qmc5883p_eil_dev_t *dev, qmc5883p_eil_set_reset_t mode)
{
    CHECK_ARG(dev && mode <= QMC5883P_EIL_SET_RESET_OFF);

    return update_register(dev, REG_CTRL2, MASK_SET_RESET, mode << BIT_SET_RESET);
}

esp_err_t qmc5883p_eil_self_test_start(qmc5883p_eil_dev_t *dev)
{
    CHECK_ARG(dev);

    return update_register(dev, REG_CTRL2, MASK_SELF_TEST, MASK_SELF_TEST);
}

esp_err_t qmc5883p_eil_get_gain(qmc5883p_eil_dev_t *dev, qmc5883p_eil_gain_t *val)
{
    CHECK_ARG(dev && val);

    uint8_t raw;
    CHECK(read_field(dev, REG_CTRL2, MASK_RNG, BIT_RNG, &raw));
    *val = (qmc5883p_eil_gain_t)raw;
    return ESP_OK;
}

esp_err_t qmc5883p_eil_set_gain(qmc5883p_eil_dev_t *dev, qmc5883p_eil_gain_t gain)
{
    CHECK_ARG(dev && gain <= QMC5883P_EIL_GAIN_15000);

    // Never carry over SOFT_RST / SELF_TEST from the read-back value
    uint8_t ctrl2;
    CHECK(read_register(dev, REG_CTRL2, &ctrl2));
    ctrl2 &= ~(MASK_RNG | MASK_SELF_TEST | MASK_SOFT_RST);
    CHECK(write_register(dev, REG_CTRL2, ctrl2 | (gain << BIT_RNG)));
    dev->gain = gain_values[gain];
    return ESP_OK;
}

esp_err_t qmc5883p_eil_data_is_ready(qmc5883p_eil_dev_t *dev, bool *val)
{
    CHECK_ARG(dev && val);

    uint8_t reg;
    CHECK(read_register(dev, REG_STAT, &reg));
    *val = (reg & MASK_DRDY) != 0;
    return ESP_OK;
}

esp_err_t qmc5883p_eil_data_is_overflow(qmc5883p_eil_dev_t *dev, bool *val)
{
    CHECK_ARG(dev && val);

    uint8_t reg;
    CHECK(read_register(dev, REG_STAT, &reg));
    *val = (reg & MASK_OVFL) != 0;
    return ESP_OK;
}

esp_err_t qmc5883p_eil_get_raw_data(qmc5883p_eil_dev_t *dev, qmc5883p_eil_raw_data_t *data)
{
    CHECK_ARG(dev && data);

    if (dev->opmode == QMC5883P_EIL_MODE_SINGLE)
    {
        CHECK(qmc5883p_eil_set_opmode(dev, dev->opmode));
        uint64_t start = esp_timer_get_time();
        bool dready = false;
        do
        {
            CHECK(qmc5883p_eil_data_is_ready(dev, &dready));
            if (!dready && timeout_expired(start, CONFIG_QMC5883P_EIL_MEAS_TIMEOUT))
                return ESP_ERR_TIMEOUT;
        }
        while (!dready);
    }

    uint8_t buf[6];
    CHECK(i2c_bus_read_bytes(dev->dev_handle, REG_DX_L, 6, buf));

    data->x = (int16_t)(((uint16_t)buf[REG_DX_H - REG_DX_L] << 8) | buf[REG_DX_L - REG_DX_L]);
    data->y = (int16_t)(((uint16_t)buf[REG_DY_H - REG_DX_L] << 8) | buf[REG_DY_L - REG_DX_L]);
    data->z = (int16_t)(((uint16_t)buf[REG_DZ_H - REG_DX_L] << 8) | buf[REG_DZ_L - REG_DX_L]);

    return ESP_OK;
}

esp_err_t qmc5883p_eil_raw_to_mg(const qmc5883p_eil_dev_t *dev, const qmc5883p_eil_raw_data_t *raw, qmc5883p_eil_data_t *mg)
{
    CHECK_ARG(dev && raw && mg);

    mg->x = raw->x * dev->gain;
    mg->y = raw->y * dev->gain;
    mg->z = raw->z * dev->gain;

    return ESP_OK;
}

esp_err_t qmc5883p_eil_get_data(qmc5883p_eil_dev_t *dev, qmc5883p_eil_data_t *data)
{
    CHECK_ARG(data);

    qmc5883p_eil_raw_data_t raw;

    CHECK(qmc5883p_eil_get_raw_data(dev, &raw));
    CHECK(qmc5883p_eil_raw_to_mg(dev, &raw, data));

    return ESP_OK;
}
