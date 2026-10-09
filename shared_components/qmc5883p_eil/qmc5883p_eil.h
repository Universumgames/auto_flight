/**
 * @file qmc5883p_eil.h
 * @defgroup qmc5883p_eil qmc5883p_eil
 * @{
 *
 * ESP-IDF driver for the 3-axis digital compass QMC5883P.
 *
 * The API mirrors esp-idf-lib/hmc5883l (Ruslan V. Uss), so code written
 * against that driver can switch over by renaming hmc5883l_ -> qmc5883p_eil_
 * and HMC5883L_ -> QMC5883P_EIL_. Differences to the HMC5883L:
 *  - there is no data lock bit, qmc5883p_eil_data_is_overflow() replaces
 *    hmc5883l_data_is_locked()
 *  - the bias (self test) configuration is replaced by the set/reset mode and
 *    qmc5883p_eil_self_test_start()
 *  - gains are named by their LSB/Gauss sensitivity like on the HMC5883L, but
 *    the available values differ
 *
 * The "_eil" suffix keeps the symbols apart from the duruofu qmc5883p driver.
 */
#ifndef __QMC5883P_EIL_H__
#define __QMC5883P_EIL_H__

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>
#include <i2c_bus.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QMC5883P_EIL_ADDR 0x2c //!< I2C address

#define QMC5883P_EIL_ID 0x80 //!< Chip ID

/**
 * Device operating mode
 *
 * The datasheet requires passing through suspend mode when switching between
 * the other modes, qmc5883p_eil_set_opmode() takes care of that.
 */
typedef enum
{
    QMC5883P_EIL_MODE_SUSPEND = 0, //!< Suspend mode, no measurements, default after POR / soft reset
    QMC5883P_EIL_MODE_NORMAL,      //!< Normal mode, periodic measurements at the configured data rate
    QMC5883P_EIL_MODE_SINGLE,      //!< Single measurement mode, returns to suspend after one measurement
    QMC5883P_EIL_MODE_CONTINUOUS   //!< Continuous mode, measures without sleep time (highest data rate)
} qmc5883p_eil_opmode_t;

/**
 * Number of samples averaged per measurement output (OSR2, down sampling)
 */
typedef enum
{
    QMC5883P_EIL_SAMPLES_1 = 0, //!< 1 sample, default
    QMC5883P_EIL_SAMPLES_2,     //!< 2 samples
    QMC5883P_EIL_SAMPLES_4,     //!< 4 samples
    QMC5883P_EIL_SAMPLES_8      //!< 8 samples
} qmc5883p_eil_samples_averaged_t;

/**
 * Over sample rate of the internal digital filter (OSR1)
 *
 * Larger values lead to smaller filter bandwidth, less in-band noise and
 * higher power consumption.
 */
typedef enum
{
    QMC5883P_EIL_OSR_8 = 0, //!< Over sample rate 8, default
    QMC5883P_EIL_OSR_4,     //!< Over sample rate 4
    QMC5883P_EIL_OSR_2,     //!< Over sample rate 2
    QMC5883P_EIL_OSR_1      //!< Over sample rate 1
} qmc5883p_eil_oversampling_t;

/**
 * Data output rate in normal measurement mode
 */
typedef enum
{
    QMC5883P_EIL_DATA_RATE_10 = 0, //!< 10 Hz, default
    QMC5883P_EIL_DATA_RATE_50,     //!< 50 Hz
    QMC5883P_EIL_DATA_RATE_100,    //!< 100 Hz
    QMC5883P_EIL_DATA_RATE_200     //!< 200 Hz
} qmc5883p_eil_data_rate_t;

/**
 * Set/reset mode, controls whether the sensor offset is renewed while measuring
 */
typedef enum
{
    QMC5883P_EIL_SET_RESET_ON = 0, //!< Set and reset on, default
    QMC5883P_EIL_SET_ONLY_ON,      //!< Set only on, offset is not renewed
    QMC5883P_EIL_SET_RESET_OFF     //!< Set and reset off, offset is not renewed
} qmc5883p_eil_set_reset_t;

/**
 * Device gain, named by sensitivity in LSB/Gauss
 */
typedef enum
{
    QMC5883P_EIL_GAIN_1000 = 0, //!< 1.00 mG/LSb, range -30..+30 G, default
    QMC5883P_EIL_GAIN_2500,     //!< 0.40 mG/LSb, range -12..+12 G
    QMC5883P_EIL_GAIN_3750,     //!< 0.27 mG/LSb, range -8..+8 G
    QMC5883P_EIL_GAIN_15000     //!< 0.07 mG/LSb, range -2..+2 G
} qmc5883p_eil_gain_t;

/**
 * Device descriptor
 */
typedef struct
{
    i2c_bus_handle_t bus_handle;        //!< I2C bus handle
    i2c_bus_device_handle_t dev_handle; //!< I2C device handle
    qmc5883p_eil_opmode_t opmode;       //!< Operating mode
    float gain;                         //!< Gain, mG/LSb
} qmc5883p_eil_dev_t;

/**
 * Raw measurement result
 */
typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} qmc5883p_eil_raw_data_t;

/**
 * Measurement result, milligauss
 */
typedef struct
{
    float x;
    float y;
    float z;
} qmc5883p_eil_data_t;

/**
 * @brief Initialize device descriptor
 *
 * @param dev Device descriptor
 * @param bus_handle I2C bus handle from espressif__i2c_bus
 * @return `ESP_OK` on success
 */
esp_err_t qmc5883p_eil_init_desc(qmc5883p_eil_dev_t *dev, i2c_bus_handle_t bus_handle);

/**
 * @brief Free device descriptor
 *
 * @param dev Device descriptor
 * @return `ESP_OK` on success
 */
esp_err_t qmc5883p_eil_free_desc(qmc5883p_eil_dev_t *dev);

/**
 * @brief Initialize device
 *
 * Verifies the chip ID, performs a soft reset and writes the axis sign
 * configuration recommended by the datasheet. The device is in suspend mode
 * afterwards, select a mode with qmc5883p_eil_set_opmode().
 *
 * @param dev Device descriptor
 * @return `ESP_OK` on success
 */
esp_err_t qmc5883p_eil_init(qmc5883p_eil_dev_t *dev);

/**
 * @brief Perform a soft reset, restores the default value of all registers
 *
 * @param dev Device descriptor
 * @return `ESP_OK` on success
 */
esp_err_t qmc5883p_eil_soft_reset(qmc5883p_eil_dev_t *dev);

/**
 * @brief Get operating mode
 *
 * @param dev Device descriptor
 * @param[out] val Operating mode
 * @return `ESP_OK` on success
 */
esp_err_t qmc5883p_eil_get_opmode(qmc5883p_eil_dev_t *dev, qmc5883p_eil_opmode_t *val);

/**
 * @brief Set operating mode
 *
 * @param dev Device descriptor
 * @param mode Operating mode
 * @return `ESP_OK` on success
 */
esp_err_t qmc5883p_eil_set_opmode(qmc5883p_eil_dev_t *dev, qmc5883p_eil_opmode_t mode);

/**
 * @brief Get number of samples averaged per measurement output
 *
 * @param dev Device descriptor
 * @param[out] val Number of samples
 * @return `ESP_OK` on success
 */
esp_err_t qmc5883p_eil_get_samples_averaged(qmc5883p_eil_dev_t *dev, qmc5883p_eil_samples_averaged_t *val);

/**
 * @brief Set number of samples averaged per measurement output
 *
 * @param dev Device descriptor
 * @param samples Number of samples
 * @return `ESP_OK` on success
 */
esp_err_t qmc5883p_eil_set_samples_averaged(qmc5883p_eil_dev_t *dev, qmc5883p_eil_samples_averaged_t samples);

/**
 * @brief Get over sample rate of the internal digital filter
 *
 * @param dev Device descriptor
 * @param[out] val Over sample rate
 * @return `ESP_OK` on success
 */
esp_err_t qmc5883p_eil_get_oversampling(qmc5883p_eil_dev_t *dev, qmc5883p_eil_oversampling_t *val);

/**
 * @brief Set over sample rate of the internal digital filter
 *
 * @param dev Device descriptor
 * @param osr Over sample rate
 * @return `ESP_OK` on success
 */
esp_err_t qmc5883p_eil_set_oversampling(qmc5883p_eil_dev_t *dev, qmc5883p_eil_oversampling_t osr);

/**
 * @brief Get data output rate in normal measurement mode
 *
 * @param dev Device descriptor
 * @param[out] val Data output rate
 * @return `ESP_OK` on success
 */
esp_err_t qmc5883p_eil_get_data_rate(qmc5883p_eil_dev_t *dev, qmc5883p_eil_data_rate_t *val);

/**
 * @brief Set data output rate in normal measurement mode
 *
 * @param dev Device descriptor
 * @param rate Data output rate
 * @return `ESP_OK` on success
 */
esp_err_t qmc5883p_eil_set_data_rate(qmc5883p_eil_dev_t *dev, qmc5883p_eil_data_rate_t rate);

/**
 * @brief Get set/reset mode
 *
 * @param dev Device descriptor
 * @param[out] val Set/reset mode
 * @return `ESP_OK` on success
 */
esp_err_t qmc5883p_eil_get_set_reset(qmc5883p_eil_dev_t *dev, qmc5883p_eil_set_reset_t *val);

/**
 * @brief Set set/reset mode
 *
 * @param dev Device descriptor
 * @param mode Set/reset mode
 * @return `ESP_OK` on success
 */
esp_err_t qmc5883p_eil_set_set_reset(qmc5883p_eil_dev_t *dev, qmc5883p_eil_set_reset_t mode);

/**
 * @brief Enable the self test stimulus for the next measurement
 *
 * Only works in continuous mode, the bit is cleared automatically once the
 * data is updated. See datasheet for the self test procedure.
 *
 * @param dev Device descriptor
 * @return `ESP_OK` on success
 */
esp_err_t qmc5883p_eil_self_test_start(qmc5883p_eil_dev_t *dev);

/**
 * @brief Get device gain
 *
 * @param dev Device descriptor
 * @param[out] val Current gain
 * @return `ESP_OK` on success
 */
esp_err_t qmc5883p_eil_get_gain(qmc5883p_eil_dev_t *dev, qmc5883p_eil_gain_t *val);

/**
 * @brief Set device gain
 * @param dev Device descriptor
 * @param gain Gain
 * @return `ESP_OK` on success
 */
esp_err_t qmc5883p_eil_set_gain(qmc5883p_eil_dev_t *dev, qmc5883p_eil_gain_t gain);

/**
 * @brief Get data state
 *
 * @param dev Device descriptor
 * @param[out] val true when data of all three axes is ready
 * @return `ESP_OK` on success
 */
esp_err_t qmc5883p_eil_data_is_ready(qmc5883p_eil_dev_t *dev, bool *val);

/**
 * @brief Get overflow state
 *
 * @param dev Device descriptor
 * @param[out] val true when an axis output exceeded [-30000, 30000] LSB
 * @return `ESP_OK` on success
 */
esp_err_t qmc5883p_eil_data_is_overflow(qmc5883p_eil_dev_t *dev, bool *val);

/**
 * @brief Get raw magnetic data
 *
 * In single measurement mode this triggers a measurement and waits for it
 * (up to CONFIG_QMC5883P_EIL_MEAS_TIMEOUT).
 *
 * @param dev Device descriptor
 * @param[out] data Raw data
 * @return `ESP_OK` on success
 */
esp_err_t qmc5883p_eil_get_raw_data(qmc5883p_eil_dev_t *dev, qmc5883p_eil_raw_data_t *data);

/**
 * @brief Convert raw magnetic data to milligausses
 *
 * @param dev Device descriptor
 * @param raw Source raw data
 * @param[out] mg Converted data
 */
esp_err_t qmc5883p_eil_raw_to_mg(const qmc5883p_eil_dev_t *dev, const qmc5883p_eil_raw_data_t *raw, qmc5883p_eil_data_t *mg);

/**
 * @brief Get magnetic data in milligausses
 *
 * @param dev Device descriptor
 * @param[out] data Magnetic data
 * @return `ESP_OK` on success
 */
esp_err_t qmc5883p_eil_get_data(qmc5883p_eil_dev_t *dev, qmc5883p_eil_data_t *data);

#ifdef __cplusplus
}
#endif

/**@}*/

#endif /* __QMC5883P_EIL_H__ */
