#include <string.h>
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "hmc5983.h"
#include "imu_util.h"

#define HMC5983_I2C_ADDRESS               (0x1E)
#define HMC5983_I2C_CLK_FREQ              400000

// Register Addresses
#define HMC5983_REG_CONF_A                0x00
#define HMC5983_REG_CONF_B                0x01
#define HMC5983_REG_MODE                  0x02
#define HMC5983_REG_DATA_X_MSB            0x03
#define HMC5983_REG_STATUS                0x09

const static char* TAG = "hmc5983";

static i2c_master_bus_handle_t i2c_bus;
static i2c_master_dev_handle_t hmc5983_dev;

static void hmc5983_i2c_init(void)
{
  i2c_master_bus_config_t bus_config = {
    .clk_source   = I2C_CLK_SRC_DEFAULT,
    .i2c_port     = I2C_NUM_0,
    .sda_io_num   = 5, // Match your SDA
    .scl_io_num   = 4, // Match your SCL
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
  };

  ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

  i2c_device_config_t dev_cfg = {
    .dev_addr_length  = I2C_ADDR_BIT_LEN_7,
    .device_address   = HMC5983_I2C_ADDRESS,
    .scl_speed_hz     = HMC5983_I2C_CLK_FREQ,
  };

  ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &dev_cfg, &hmc5983_dev));
}

static bool hmc5983_write_reg(hmc5983Mag* mag, uint8_t reg, uint8_t data)
{
  uint8_t buf[2] = { reg, data };
  mag->num_transactions++;
  esp_err_t ret = i2c_master_transmit(hmc5983_dev, buf, 2, IMU_MSEC_TO_TICKS(100));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "hmc5983_write_reg failed: %s", esp_err_to_name(ret));
    mag->num_failed++;
    return false;
  }
  return true;
}

static bool hmc5983_read_reg(hmc5983Mag* mag, uint8_t reg, uint8_t* data, uint8_t len)
{
  mag->num_transactions++;
  esp_err_t ret = i2c_master_transmit_receive(hmc5983_dev, &reg, 1, data, len, IMU_MSEC_TO_TICKS(100));
  if (ret != ESP_OK) {
    mag->num_failed++;
    ESP_LOGE(TAG, "hmc5983_read_reg failed: %s", esp_err_to_name(ret));
    return false;
  }
  return true;
}

void hmc5983_init(hmc5983Mag* mag, float* lsb)
{
  mag->num_transactions = 0;
  mag->num_failed       = 0;
  *lsb = HMC5983_MAG_LSB;

  hmc5983_i2c_init();

  // 1. Config A: 8-sample avg, 220Hz update rate, Temperature sensor ENABLED
  // 1(Temp) 11(8-avg) 111(220Hz) 00(Normal) = 0xFC
  if(hmc5983_write_reg(mag, HMC5983_REG_CONF_A, 0xFC) == false)
  {
    ESP_LOGI(TAG, "init write failed. retrying");
    if(hmc5983_write_reg(mag, HMC5983_REG_CONF_A, 0xFC) == false)
    {
      ESP_LOGE(TAG, "hmc5983_write_reg init failed");
    }
  }

  // 2. Config B: Range +/- 1.3 Gauss (Default)
  hmc5983_write_reg(mag, HMC5983_REG_CONF_B, 0x20);

  // 3. Mode: Continuous Conversion
  hmc5983_write_reg(mag, HMC5983_REG_MODE, 0x00);
}

void hmc5983_read(hmc5983Mag* mag, int16_t m[3])
{
  uint8_t data[6];
  // HMC5983 auto-increments register address
  if(hmc5983_read_reg(mag, HMC5983_REG_DATA_X_MSB, data, 6)) {
    // IMPORTANT: HMC5983 register order is X-MSB, X-LSB, Z-MSB, Z-LSB, Y-MSB, Y-LSB
    m[0] = (int16_t)((data[0] << 8) | data[1]); // X
    m[2] = (int16_t)((data[2] << 8) | data[3]); // Z
    m[1] = (int16_t)((data[4] << 8) | data[5]); // Y
  }
}
