#ifndef ESP_HAL_H
#define ESP_HAL_H

// include RadioLib
#include <RadioLib.h>

// support the native ESP32 and ESP32-S3 targets used by this project
#if !CONFIG_IDF_TARGET_ESP32 && !CONFIG_IDF_TARGET_ESP32S3
  #error This HAL currently supports ESP32 and ESP32-S3 targets.
#endif

// include all of the dependencies
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

// define Arduino-style macros
#define LOW                         (0x0)
#define HIGH                        (0x1)
#define INPUT                       GPIO_MODE_INPUT
#define OUTPUT                      GPIO_MODE_OUTPUT
#define RISING                      GPIO_INTR_POSEDGE
#define FALLING                     GPIO_INTR_NEGEDGE

#define NOP()                       asm volatile ("nop")

// create a new ESP-IDF hardware abstraction layer
// the HAL must inherit from the base RadioLibHal class
// and implement all of its virtual methods
class EspHal : public RadioLibHal {
  public:
    // default constructor - initializes the base HAL and any needed private members
    EspHal(int8_t sck, int8_t miso, int8_t mosi)
      : RadioLibHal(INPUT, OUTPUT, LOW, HIGH, RISING, FALLING),
      spiSCK(sck), spiMISO(miso), spiMOSI(mosi)  {
    }

    void init() override {
      // we only need to init the SPI here
      spiBegin();
    }

    void term() override {
      // we only need to stop the SPI here
      spiEnd();
    }

    // GPIO-related methods (pinMode, digitalWrite etc.) should check
    // RADIOLIB_NC as an alias for non-connected pins
    void pinMode(uint32_t pin, uint32_t mode) override {
      if(pin == RADIOLIB_NC) {
        return;
      }

      gpio_config_t conf = {};
      conf.pin_bit_mask = (1ULL << pin);
      conf.mode = (gpio_mode_t)mode;
      conf.pull_up_en = GPIO_PULLUP_DISABLE;
      conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
      conf.intr_type = GPIO_INTR_DISABLE;
      gpio_config(&conf);
    }

    void digitalWrite(uint32_t pin, uint32_t value) override {
      if(pin == RADIOLIB_NC) {
        return;
      }

      gpio_set_level((gpio_num_t)pin, value);
    }

    uint32_t digitalRead(uint32_t pin) override {
      if(pin == RADIOLIB_NC) {
        return(0);
      }

      return(gpio_get_level((gpio_num_t)pin));
    }

    void attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) override {
      if(interruptNum == RADIOLIB_NC) {
        return;
      }

      gpio_config_t io_conf{};
      io_conf.intr_type = (gpio_int_type_t) GPIO_INTR_POSEDGE;
      io_conf.mode = GPIO_MODE_INPUT;
      io_conf.pin_bit_mask = 1ULL << CONFIG_LORA_DIO0_PIN;
      io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
      io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
      gpio_config(&io_conf);

      esp_err_t err = gpio_install_isr_service(0);
      if(err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG_LORA_ESP_HAL, "Failed to install GPIO ISR service: %s", esp_err_to_name(err));
        return;
      }

      err = gpio_isr_handler_add((gpio_num_t)interruptNum, (void (*)(void*))interruptCb, nullptr);
      if(err != ESP_OK) {
        ESP_LOGE(TAG_LORA_ESP_HAL, "Failed to add ISR handler for DIO0 pin %d: %s", interruptNum, esp_err_to_name(err));
      }
    }

    void detachInterrupt(uint32_t interruptNum) override {
      if(interruptNum == RADIOLIB_NC) {
        return;
      }

      gpio_isr_handler_remove((gpio_num_t)interruptNum);
      gpio_wakeup_disable((gpio_num_t)interruptNum);
      gpio_set_intr_type((gpio_num_t)interruptNum, GPIO_INTR_DISABLE);
    }

    void delay(unsigned long ms) override {
      vTaskDelay(ms / portTICK_PERIOD_MS);
      ESP_LOGI(TAG_LORA_ESP_HAL, "Delaying for %u ms", ms);
    }

    void delayMicroseconds(unsigned long us) override {
      uint64_t m = (uint64_t)esp_timer_get_time();
      if(us) {
        uint64_t e = (m + us);
        if(m > e) { // overflow
          while((uint64_t)esp_timer_get_time() > e) {
            NOP();
          }
        }
        while((uint64_t)esp_timer_get_time() < e) {
          NOP();
        }
      }
    }

    unsigned long millis() override {
      return((unsigned long)(esp_timer_get_time() / 1000ULL));
    }

    unsigned long micros() override {
      return((unsigned long)(esp_timer_get_time()));
    }

    long pulseIn(uint32_t pin, uint32_t state, unsigned long timeout) override {
      if(pin == RADIOLIB_NC) {
        return(0);
      }

      this->pinMode(pin, INPUT);
      uint32_t start = this->micros();
      uint32_t curtick = this->micros();

      while(this->digitalRead(pin) == state) {
        if((this->micros() - curtick) > timeout) {
          return(0);
        }
      }

      return(this->micros() - start);
    }

    void spiBegin() override {
      if(this->spiBusInitialized) {
        return;
      }
      
      ESP_LOGI(TAG_LORA_ESP_HAL, "Initializing SPI");

      // initialize pins
      this->pinMode(this->spiSCK, OUTPUT);
      this->pinMode(this->spiMISO, INPUT);
      this->pinMode(this->spiMOSI, OUTPUT);

      spi_bus_config_t busCfg = {};
      busCfg.sclk_io_num = this->spiSCK;
      busCfg.mosi_io_num = this->spiMOSI;
      busCfg.miso_io_num = this->spiMISO;
      busCfg.quadwp_io_num = -1;
      busCfg.quadhd_io_num = -1;
      busCfg.max_transfer_sz = 0;

      esp_err_t err = spi_bus_initialize(this->spiHost, &busCfg, SPI_DMA_CH_AUTO);
      if(err == ESP_OK) {
        this->spiBusOwned = true;
      }
      else if(err == ESP_ERR_INVALID_STATE) {
        // another component already initialized the bus; reuse it
        this->spiBusOwned = false;
      }
      else {
        ESP_LOGE(TAG_LORA_ESP_HAL, "Failed to initialize SPI bus: %s", esp_err_to_name(err));
        return;
      }

      spi_device_interface_config_t devCfg = {};
      devCfg.clock_speed_hz = 2000000;
      devCfg.mode = 0;
      devCfg.spics_io_num = -1;
      devCfg.queue_size = 1;

      err = spi_bus_add_device(this->spiHost, &devCfg, &this->spiDevice);
      if(err != ESP_OK) {
        ESP_LOGE(TAG_LORA_ESP_HAL, "Failed to add SPI device: %s", esp_err_to_name(err));
        if(this->spiBusOwned) {
          spi_bus_free(this->spiHost);
        }
        this->spiBusOwned = false;
        this->spiDevice = nullptr;
        return;
      }

      this->spiBusInitialized = true;
      ESP_LOGI(TAG_LORA_ESP_HAL, "SPI initialized");
    }

    void spiBeginTransaction() override {
      // not needed - the IDF SPI driver handles transactions internally
    }

    uint8_t spiTransferByte(uint8_t b) {
      if(this->spiDevice == nullptr) {
        return(0);
      }

      uint8_t rx = 0;
      spi_transaction_t trans = {};
      trans.length = 8;
      trans.rxlength = 8;
      trans.tx_buffer = &b;
      trans.rx_buffer = &rx;

      esp_err_t err = spi_device_polling_transmit(this->spiDevice, &trans);
      if(err != ESP_OK) {
        ESP_LOGE(TAG_LORA_ESP_HAL, "SPI transfer failed: %s", esp_err_to_name(err));
        return(0);
      }

      return(rx);
    }

    void spiTransfer(uint8_t* out, size_t len, uint8_t* in) override {
      for(size_t i = 0; i < len; i++) {
        uint8_t tx = (out != nullptr) ? out[i] : 0;
        uint8_t rx = this->spiTransferByte(tx);
        if(in != nullptr) {
          in[i] = rx;
        }
      }
    }

    void spiEndTransaction() override {
      // nothing needs to be done here
    }

    void spiEnd() override {
      if(this->spiDevice != nullptr) {
        spi_bus_remove_device(this->spiDevice);
        this->spiDevice = nullptr;
      }

      if(this->spiBusOwned) {
        spi_bus_free(this->spiHost);
      }

      this->spiBusInitialized = false;
      this->spiBusOwned = false;
    }

  private:
    // the HAL can contain any additional private members
    int8_t spiSCK;
    int8_t spiMISO;
    int8_t spiMOSI;
    spi_host_device_t spiHost = SPI2_HOST;
    spi_device_handle_t spiDevice = nullptr;
    bool spiBusInitialized = false;
    bool spiBusOwned = false;
  
    const char* TAG_LORA_ESP_HAL = "LoRaEspHal";
};

#endif
