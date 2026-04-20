#include <string.h>
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "qmc5883l.h"
#include "imu_util.h"

////////////////////////////////////////////////////////////////////////////////
//
// internal defines
//
////////////////////////////////////////////////////////////////////////////////
#define SENSORS_GAUSS_TO_MICROTESLA       (100)  

#define QMC5883_I2C_ADDRESS               (0x0d)

#define QMC5883L_I2C_MASTER_SCL_IO        4
#define QMC5883L_I2C_MASTER_SDA_IO        5
#define QMC5883L_I2C_CLK_FREQ             400000

const static char* TAG = "qmc5883l";

static i2c_master_bus_handle_t i2c_bus;
static i2c_master_dev_handle_t qmc5883l_dev;

////////////////////////////////////////////////////////////////////////////////
//
// private utilities
//
////////////////////////////////////////////////////////////////////////////////
static void
qmc5883l_i2c_init(void)
{
  i2c_master_bus_config_t bus_config = {
    .clk_source   = I2C_CLK_SRC_DEFAULT,
    .i2c_port     = I2C_NUM_0,
    .sda_io_num   = QMC5883L_I2C_MASTER_SDA_IO,
    .scl_io_num   = QMC5883L_I2C_MASTER_SCL_IO,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,   // already pulled up on the chinese mpu9255 breakout board
  };

  esp_err_t ret = i2c_new_master_bus(&bus_config, &i2c_bus);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
  }

  i2c_device_config_t dev_cfg =
  {
    .dev_addr_length  = I2C_ADDR_BIT_LEN_7,
    .device_address   = QMC5883_I2C_ADDRESS,
    .scl_speed_hz     = QMC5883L_I2C_CLK_FREQ,
  };

  ret = i2c_master_bus_add_device(i2c_bus, &dev_cfg, &qmc5883l_dev);
  if(ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to add QMC5883L device: %s", esp_err_to_name(ret));
  }
}

static inline bool
qmc5883l_write_reg(qmc5883lMag* mag, uint8_t reg, uint8_t data)
{
  uint8_t buf[2] = { reg, data };

  mag->num_transactions++;

  esp_err_t ret = i2c_master_transmit(qmc5883l_dev,
      buf, 2,
      IMU_MSEC_TO_TICKS(100));

  if (ret != ESP_OK)
  {
    mag->num_failed++;
    ESP_LOGE(TAG, "qmc5883_write_reg failed: %s", esp_err_to_name(ret));
    return false;
  }
  return true;
}

static inline bool
qmc5883l_read_reg(qmc5883lMag* mag, uint8_t reg, uint8_t* data, uint8_t len)
{
  mag->num_transactions++;

  esp_err_t ret = i2c_master_transmit_receive(qmc5883l_dev,
      &reg, 1,
      data, len,
      IMU_MSEC_TO_TICKS(100));

  if (ret != ESP_OK)
  {
    mag->num_failed++;
    ESP_LOGE(TAG, "qmc5883_read_reg failed: %s", esp_err_to_name(ret));
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
//
// public utilities
//
////////////////////////////////////////////////////////////////////////////////
void
qmc5883l_init(qmc5883lMag* mag, float* lsb)
{
  mag->num_transactions = 0;
  mag->num_failed       = 0;

  *lsb = QMC5883L_MAG_LSB;

  qmc5883l_i2c_init();

  // 1. Take out of standby: Continuous mode, 200Hz, 8G range, 512 oversampling
  if(qmc5883l_write_reg(mag, 0x09, 0x1d) == false)
  {
    ESP_LOGI(TAG, "init write failed. retrying");
    if(qmc5883l_write_reg(mag, 0x09, 0x1d) == false)
    {
      ESP_LOGE(TAG, "qmc5883_write_reg init failed");
    }
  }

  // 2. Set/Reset Period (Required by datasheet)
  qmc5883l_write_reg(mag, 0x0b, 0x01);
}

void
qmc5883l_read(qmc5883lMag* mag, int16_t m[3])
{
  uint8_t   data[6];

  qmc5883l_read_reg(mag, 0x00, data, 6);

  m[0] = (int16_t)((int16_t)data[0] | ((int16_t)data[1] << 8));
  m[1] = (int16_t)((int16_t)data[2] | ((int16_t)data[3] << 8));
  m[2] = (int16_t)((int16_t)data[4] | ((int16_t)data[5] << 8));
}
