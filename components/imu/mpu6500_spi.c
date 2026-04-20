#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "mpu6500_spi.h"
#include "imu_util.h"

#define MPU6500_SPI_CLK       0
#define MPU6500_SPI_MISO      1
#define MPU6500_SPI_MOSI      2
#define MPU6500_SPI_CS        3

#define MPU6500_SPI_HOST      SPI2_HOST

#define MPU6500_MAX_TRANSFER_SIZE     32

static spi_device_handle_t _mpu6500_spi_slow;
static spi_device_handle_t _mpu6500_spi_fast;

static uint8_t _tx_buf[MPU6500_MAX_TRANSFER_SIZE];
static uint8_t _rx_buf[MPU6500_MAX_TRANSFER_SIZE];

static bool _use_fast_speed = false;

void
mpu6500_spi_init(void)
{
  spi_bus_config_t buscfg = {
    .mosi_io_num      = MPU6500_SPI_MOSI,
    .miso_io_num      = MPU6500_SPI_MISO,
    .sclk_io_num      = MPU6500_SPI_CLK,
    .quadwp_io_num    = -1,
    .quadhd_io_num    = -1,
    .max_transfer_sz  = MPU6500_MAX_TRANSFER_SIZE,
  };
  ESP_ERROR_CHECK(spi_bus_initialize(MPU6500_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

  spi_device_interface_config_t slow_cfg = 
  {
    .clock_speed_hz   = 1*1000*1000,
    .mode             = 3,
    .spics_io_num     = -1,
    .queue_size       = 1,
    .pre_cb           = NULL,
    .cs_ena_pretrans  = 0,
    .cs_ena_posttrans = 0,
    .input_delay_ns   = 0,
  };
  ESP_ERROR_CHECK(spi_bus_add_device(MPU6500_SPI_HOST, &slow_cfg, &_mpu6500_spi_slow));

  spi_device_interface_config_t fast_cfg = 
  {
    .clock_speed_hz   = 20*1000*1000,
    .mode             = 3,
    .spics_io_num     = -1,
    .queue_size       = 1,
    .pre_cb           = NULL,
    .cs_ena_pretrans  = 0,
    .cs_ena_posttrans = 0,
    .input_delay_ns   = 0,
  };
  ESP_ERROR_CHECK(spi_bus_add_device(MPU6500_SPI_HOST, &fast_cfg, &_mpu6500_spi_fast));

  gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << MPU6500_SPI_CS),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = 1,
    .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&io_conf);
  gpio_set_level(MPU6500_SPI_CS, 1);
}

bool
mpu6500_spi_read(uint8_t reg, uint8_t *data, int len)
{
  spi_transaction_t trans = {0};

  _tx_buf[0] = reg | 0x80;    // MSB 1 for read

  trans.tx_buffer = _tx_buf;
  trans.rx_buffer = _rx_buf;
  trans.length = (len + 1) * 8;

  gpio_set_level(MPU6500_SPI_CS, 0);
  ESP_ERROR_CHECK(spi_device_polling_transmit(_use_fast_speed ? _mpu6500_spi_fast : _mpu6500_spi_slow, &trans));
  gpio_set_level(MPU6500_SPI_CS, 1);
  memcpy(data, &_rx_buf[1], len);
  return true;
}

bool
mpu6500_spi_write(uint8_t reg, const uint8_t *data, int len)
{
  spi_transaction_t trans = {0};

  _tx_buf[0] = reg & 0x7f;    // MSB 0 for write
  memcpy(&_tx_buf[1], data, len);

  trans.tx_buffer = _tx_buf;
  trans.rx_buffer = NULL;
  trans.length = (len + 1) * 8;

  gpio_set_level(MPU6500_SPI_CS, 0);
  ESP_ERROR_CHECK(spi_device_polling_transmit(_use_fast_speed ? _mpu6500_spi_fast : _mpu6500_spi_slow, &trans));
  gpio_set_level(MPU6500_SPI_CS, 1);
  return true;
}

void
mpu6500_spi_set_fast(bool fast)
{
  _use_fast_speed = fast;
}
