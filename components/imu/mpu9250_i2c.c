#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "mpu9250_i2c.h"
#include "imu_util.h"

#define MPU9250_I2C_ADDR        0x68
#define AK8963_I2C_ADDR         0x0c

const static char* TAG = "mpu9250_i2c";

static i2c_master_bus_handle_t i2c_bus;
static i2c_master_dev_handle_t mpu9250_dev;
static i2c_master_dev_handle_t ak8963_dev;

#if 0
static void
mpu9250_nuclear_recovery(void) {
  // 1. Set all to Input/Output Open-Drain to "listen" to the bus
  gpio_config_t ncs_conf = {
    .pin_bit_mask = (1ULL << MPU9250_I2C_MASTER_NCS_IO),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLUP_DISABLE,
  };
  gpio_config(&ncs_conf);

  gpio_config_t bus_conf = {
    .pin_bit_mask = (1ULL << MPU9250_I2C_MASTER_SDA_IO) | (1ULL << MPU9250_I2C_MASTER_SCL_IO),
    .mode = GPIO_MODE_INPUT, // Input only - no driving!
    .pull_up_en = GPIO_PULLUP_ENABLE, // Use internal as backup to your 10k
  };
  gpio_config(&bus_conf);

  int count = 0;
  // 2. Adaptive Loop
  while(true)
  {
    int sda = gpio_get_level(MPU9250_I2C_MASTER_SDA_IO);
    int scl = gpio_get_level(MPU9250_I2C_MASTER_SCL_IO);

    if (sda == 1 && scl == 1) {
      ESP_LOGI(TAG, "SUCCESS: Bus cleared after %d loops", count);
      break;
    }

    // Only log every 100ms so we don't flood the serial buffer
    if (count % 5 == 0) {
      ESP_LOGW(TAG, "WAITING: SDA=%d, SCL=%d (Iteration %d)", sda, scl, count);
    }

    // Hammer: Flip to SPI mode and back
    gpio_set_level(MPU9250_I2C_MASTER_NCS_IO, 0); 
    esp_rom_delay_us(2000); // 2ms "latch" time
    gpio_set_level(MPU9250_I2C_MASTER_NCS_IO, 1);
    esp_rom_delay_us(2000);
  }

  // 3. Final Handover
  gpio_set_level(MPU9250_I2C_MASTER_NCS_IO, 1); // Ensure nCS stays High for I2C
}
#else
static void
mpu9250_nuclear_recovery(void)
{
  //
  // sometimes during CPU reset (not Power Recycle)
  // with 500 sampling at 400Khz I2C clock,
  // when CPU gets reset (not power recycle)
  // two problems were found
  // 1. SDA line gets stuck low
  // 2. MPU9250 internal I2C handling logic gets stuck.
  // Somehow, through black box level trial and error,
  // This is the solution that was found.
  // If you ask why, no clear answers can be given to you.
  // 
  // Another solution is, at first I2C transaction failure after reset,
  // if we retry, it seems to be working fine at that too.
  //
  // both logics are implemented here and mpu9250.c
  // 
  gpio_config_t ncs_conf = {
    .pin_bit_mask = (1ULL << MPU9250_I2C_MASTER_NCS_IO),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLUP_DISABLE,
  };
  gpio_config(&ncs_conf);

  gpio_config_t bus_conf = {
    .pin_bit_mask = (1ULL << MPU9250_I2C_MASTER_SDA_IO) | (1ULL << MPU9250_I2C_MASTER_SCL_IO),
    .mode = GPIO_MODE_INPUT, // Input only - no driving!
    .pull_up_en = GPIO_PULLUP_ENABLE, // Use internal as backup to your 10k
  };
  gpio_config(&bus_conf);

  int sda = gpio_get_level(MPU9250_I2C_MASTER_SDA_IO);
  int scl = gpio_get_level(MPU9250_I2C_MASTER_SCL_IO);

  ESP_LOGI(TAG, "Bus State: SCL %d, SDA %d", scl, sda);

  // Hammer: Flip to SPI mode and back
  gpio_set_level(MPU9250_I2C_MASTER_NCS_IO, 0); 
  esp_rom_delay_us(2000); // 2ms "latch" time
  gpio_set_level(MPU9250_I2C_MASTER_NCS_IO, 1);
  esp_rom_delay_us(2000);
}
#endif

void
mpu9250_i2c_init(void)
{
  ESP_LOGI(TAG, "Trying to Recover");
  mpu9250_nuclear_recovery();
  ESP_LOGI(TAG, "Trying to Recover Complete");

  i2c_master_bus_config_t bus_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = MPU9250_I2C_NUM,
    .sda_io_num = MPU9250_I2C_MASTER_SDA_IO,
    .scl_io_num = MPU9250_I2C_MASTER_SCL_IO,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = false,   // already pulled up on the chinese mpu9255 breakout board
  };

  esp_err_t ret = i2c_new_master_bus(&bus_config, &i2c_bus);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
  }

  i2c_device_config_t dev_cfg =
  {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = MPU9250_I2C_ADDR,
    .scl_speed_hz = MPU9250_I2C_CLK_FREQ,
  };

  ret = i2c_master_bus_add_device(i2c_bus, &dev_cfg, &mpu9250_dev);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to add MPU9250 device: %s", esp_err_to_name(ret));
  }

  i2c_device_config_t mag_cfg = 
  {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = AK8963_I2C_ADDR,
    .scl_speed_hz = MPU9250_I2C_CLK_FREQ,
  };

  ret = i2c_master_bus_add_device(i2c_bus, &mag_cfg, &ak8963_dev);
  if(ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to add AK8963 device: %s", esp_err_to_name(ret));
  }
}

bool
mpu9250_i2c_read(uint8_t reg, uint8_t *data, int len)
{
  if (data == NULL || len == 0)
  {
    ESP_LOGE(TAG, "i2c_read: invalid buffer/len");
    return false;
  }

  esp_err_t ret = i2c_master_transmit_receive(mpu9250_dev,
      &reg, 1,   // send register address
      data, len, // receive data
      IMU_MSEC_TO_TICKS(100));

  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "mpu9250_i2c_read failed: %s", esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool
mpu9250_i2c_write(uint8_t reg, const uint8_t *data, int len)
{
  if (data == NULL || len == 0) {
    ESP_LOGE(TAG, "mpu9250_i2c_write: invalid buffer/len");
    return false;
  }

  // Limit payload size to something reasonable (e.g. 32 bytes)
  if (len > 32) {
    ESP_LOGE(TAG, "mpu9250_i2c_write: payload too large");
    return false;
  }

  uint8_t buf[33]; // 1 byte for reg + up to 32 bytes payload

  buf[0] = reg;
  memcpy(&buf[1], data, len);

  esp_err_t ret = i2c_master_transmit(mpu9250_dev,
      buf, len + 1,
      IMU_MSEC_TO_TICKS(100));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "mpu9250_i2c_write failed: %s", esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool
ak8963_i2c_read(uint8_t reg, uint8_t *data, int len)
{
  if (data == NULL || len == 0)
  {
    ESP_LOGE(TAG, "ak8963_i2c_read invalid buffer/len");
    return false;
  }

  esp_err_t ret = i2c_master_transmit_receive(ak8963_dev,
      &reg, 1,   // send register address
      data, len, // receive data
      IMU_MSEC_TO_TICKS(100));

  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "ak8963_i2c_read failed: %s", esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool
ak8963_i2c_write(uint8_t reg, const uint8_t *data, int len)
{
  if (data == NULL || len == 0) {
    ESP_LOGE(TAG, "ak8963_i2c_write: invalid buffer/len");
    return false;
  }

  // Limit payload size to something reasonable (e.g. 32 bytes)
  if (len > 32) {
    ESP_LOGE(TAG, "ak8963_i2c_write: payload too large");
    return false;
  }

  uint8_t buf[33]; // 1 byte for reg + up to 32 bytes payload

  buf[0] = reg;
  memcpy(&buf[1], data, len);

  esp_err_t ret = i2c_master_transmit(ak8963_dev,
      buf, len + 1,
      IMU_MSEC_TO_TICKS(100));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "ak8963_i2c_write failed: %s", esp_err_to_name(ret));
    return false;
  }
  return true;
}
