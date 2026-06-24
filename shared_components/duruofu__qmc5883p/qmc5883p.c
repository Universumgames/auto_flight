#include "qmc5883p.h"
#include "esp_log.h"
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "i2c_bus.h"

static const char *TAG = "QMC5883P";

// Sensitivity LSB/Gauss based on range (from QMC5883P datasheet)
static const float SENSITIVITY_30G = 1000.0f;
static const float SENSITIVITY_12G = 2500.0f;
static const float SENSITIVITY_8G  = 3750.0f;
static const float SENSITIVITY_2G  = 15000.0f;

esp_err_t qmc5883p_init(qmc5883p_dev_t *dev, i2c_bus_handle_t bus_handle, uint8_t i2c_addr) {
    if (!dev || !bus_handle) return ESP_ERR_INVALID_ARG;

    dev->bus_handle = bus_handle;
    dev->i2c_addr = i2c_addr;
    dev->range = QMC5883P_RNG_8G;
    dev->scale_factor_x = 1.0f / SENSITIVITY_8G;
    dev->scale_factor_y = 1.0f / SENSITIVITY_8G;
    dev->scale_factor_z = 1.0f / SENSITIVITY_8G;

    dev->dev_handle = i2c_bus_device_create(bus_handle, i2c_addr, i2c_bus_get_current_clk_speed(bus_handle));
    if (dev->dev_handle == NULL) {
        ESP_LOGE(TAG, "Failed to add I2C device");
        return ESP_FAIL;
    }

    // 1. Check Chip ID to verify connection
    uint8_t chip_id = 0;
    esp_err_t ret = qmc5883p_get_chip_id(dev, &chip_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to communicate with QMC5883P (NACK or Error)");
    } else {
        ESP_LOGI(TAG, "QMC5883P Chip ID: 0x%02X", chip_id);
        if (chip_id != QMC5883P_CHIP_ID_VAL) {
             ESP_LOGW(TAG, "Unexpected Chip ID (Expected 0x%02X)", QMC5883P_CHIP_ID_VAL);
        }
    }

    // 2. Soft Reset: set bit 7 of CTRL2, wait 50ms, then verify chip ID
    ret = qmc5883p_write_register(dev, QMC5883P_REG_CTRL2, 0x80);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(50));

    ret = qmc5883p_get_chip_id(dev, &chip_id);
    if (ret != ESP_OK || chip_id != QMC5883P_CHIP_ID_VAL) {
        ESP_LOGE(TAG, "QMC5883P not responding after soft reset");
        return ESP_FAIL;
    }

    // 3. Recommended Initialization Flow
    // Register 0x29 (SIGN) = 0x06
    ret = qmc5883p_write_register(dev, QMC5883P_REG_SIGN, 0x06);
    if (ret != ESP_OK) return ret;

    // Register 0x0B (CTRL2): Range 8G — bits 3:2 = 0b10 = 0x08
    ret = qmc5883p_write_register(dev, QMC5883P_REG_CTRL2, 0x08);
    if (ret != ESP_OK) return ret;

    // Register 0x0A (CTRL1): Continuous mode, ODR 200Hz, OSR2=3, OSR1=0
    // 0xCF = 1100 1111
    // OSR2 (7:6) = 11
    // OSR1 (5:4) = 00
    // ODR  (3:2) = 11 (200Hz)
    // MODE (1:0) = 11 (Continuous)
    ret = qmc5883p_write_register(dev, QMC5883P_REG_CTRL1, 0xCF);
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "QMC5883P initialized successfully");
    return ESP_OK;
}

esp_err_t qmc5883p_write_register(qmc5883p_dev_t *dev, uint8_t reg, uint8_t value) {
    if (!dev || !dev->dev_handle) return ESP_ERR_INVALID_ARG;
    return i2c_bus_write_byte(dev->dev_handle, reg, value);
}

esp_err_t qmc5883p_read_register(qmc5883p_dev_t *dev, uint8_t reg, uint8_t *data, size_t len) {
    if (!dev || !dev->dev_handle || !data) return ESP_ERR_INVALID_ARG;
    return i2c_bus_read_bytes(dev->dev_handle, reg, len, data);
}

esp_err_t qmc5883p_get_chip_id(qmc5883p_dev_t *dev, uint8_t *id) {
    return qmc5883p_read_register(dev, QMC5883P_REG_CHIP_ID, id, 1);
}

esp_err_t qmc5883p_soft_reset(qmc5883p_dev_t *dev) {
    esp_err_t ret = qmc5883p_write_register(dev, QMC5883P_REG_CTRL2, 0x80);
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return ret;
}

esp_err_t qmc5883p_is_data_ready(qmc5883p_dev_t *dev, bool *ready) {
    uint8_t status;
    esp_err_t ret = qmc5883p_read_register(dev, QMC5883P_REG_STATUS, &status, 1);
    if (ret == ESP_OK) {
        *ready = (status & 0x01); // Bit 0 is DRDY
    }
    return ret;
}

// Re-applies config registers without a full reset; used after EMI-induced sensor reset.
static esp_err_t qmc5883p_apply_config(qmc5883p_dev_t *dev) {
    esp_err_t ret = qmc5883p_write_register(dev, QMC5883P_REG_SIGN, 0x06);
    if (ret != ESP_OK) return ret;
    // CTRL2: range stored in dev->range, bits 3:2
    ret = qmc5883p_write_register(dev, QMC5883P_REG_CTRL2, (uint8_t)((dev->range & 0x03) << 2));
    if (ret != ESP_OK) return ret;
    // CTRL1: continuous, 200 Hz, OSR2=3, OSR1=0 (0xCF)
    return qmc5883p_write_register(dev, QMC5883P_REG_CTRL1, 0xCF);
}

esp_err_t qmc5883p_read_data(qmc5883p_dev_t *dev, qmc5883p_data_t *data) {
    if (!dev || !data) return ESP_ERR_INVALID_ARG;

    uint8_t status;
    esp_err_t ret = qmc5883p_read_register(dev, QMC5883P_REG_STATUS, &status, 1);
    if (ret != ESP_OK) return ret;

    if (status & 0x02) {
        ESP_LOGW(TAG, "Magnetic sensor overflow!");
    }

    uint8_t buf[6];
    ret = qmc5883p_read_register(dev, QMC5883P_REG_X_L, buf, 6);
    if (ret != ESP_OK) return ret;

    data->raw_x = (int16_t)((buf[1] << 8) | buf[0]);
    data->raw_y = (int16_t)((buf[3] << 8) | buf[2]);
    data->raw_z = (int16_t)((buf[5] << 8) | buf[4]);

    // If no DRDY and all axes zero, the sensor likely reset (e.g., from LoRa EMI).
    // Confirm by reading CTRL1: if it's 0x00 (suspend), silently reconfigure.
    if (!(status & 0x01) && data->raw_x == 0 && data->raw_y == 0 && data->raw_z == 0) {
        uint8_t ctrl1 = 0xFF;
        if (qmc5883p_read_register(dev, QMC5883P_REG_CTRL1, &ctrl1, 1) == ESP_OK && ctrl1 == 0x00) {
            ESP_LOGW(TAG, "Sensor reset detected (CTRL1=0), reconfiguring");
            qmc5883p_apply_config(dev);
            return ESP_ERR_INVALID_STATE;
        }
    }

    data->x = (float)data->raw_x * dev->scale_factor_x;
    data->y = (float)data->raw_y * dev->scale_factor_y;
    data->z = (float)data->raw_z * dev->scale_factor_z;

    return ESP_OK;
}

esp_err_t qmc5883p_set_mode(qmc5883p_dev_t *dev, qmc5883p_mode_t mode) {
    uint8_t ctrl1;
    esp_err_t ret = qmc5883p_read_register(dev, QMC5883P_REG_CTRL1, &ctrl1, 1);
    if (ret != ESP_OK) return ret;

    ctrl1 &= ~0x03; // Clear Mode bits (1:0)
    ctrl1 |= (mode & 0x03);

    return qmc5883p_write_register(dev, QMC5883P_REG_CTRL1, ctrl1);
}

esp_err_t qmc5883p_set_range(qmc5883p_dev_t *dev, qmc5883p_range_t range) {
    uint8_t ctrl2;
    esp_err_t ret = qmc5883p_read_register(dev, QMC5883P_REG_CTRL2, &ctrl2, 1);
    if (ret != ESP_OK) return ret;

    ctrl2 &= ~0x0C; // Clear RNG bits (3:2) -> Mask 0000 1100
    ctrl2 |= ((range & 0x03) << 2);

    ret = qmc5883p_write_register(dev, QMC5883P_REG_CTRL2, ctrl2);
    if (ret == ESP_OK) {
        dev->range = range;
        // Update scale factors
        switch(range) {
            case QMC5883P_RNG_30G: dev->scale_factor_x = 1.0f / SENSITIVITY_30G; break;
            case QMC5883P_RNG_12G: dev->scale_factor_x = 1.0f / SENSITIVITY_12G; break;
            case QMC5883P_RNG_8G:  dev->scale_factor_x = 1.0f / SENSITIVITY_8G;  break;
            case QMC5883P_RNG_2G:  dev->scale_factor_x = 1.0f / SENSITIVITY_2G;  break;
        }
        dev->scale_factor_y = dev->scale_factor_x;
        dev->scale_factor_z = dev->scale_factor_x;
    }
    return ret;
}

esp_err_t qmc5883p_set_odr(qmc5883p_dev_t *dev, qmc5883p_odr_t odr) {
    uint8_t ctrl1;
    esp_err_t ret = qmc5883p_read_register(dev, QMC5883P_REG_CTRL1, &ctrl1, 1);
    if (ret != ESP_OK) return ret;

    ctrl1 &= ~0x0C; // Clear ODR bits (3:2)
    ctrl1 |= ((odr & 0x03) << 2);

    return qmc5883p_write_register(dev, QMC5883P_REG_CTRL1, ctrl1);
}
