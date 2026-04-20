#include "mpu6500.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "imu_util.h"
#include "mpu6500_spi.h"

const static char* TAG = "mpu6500";

static const float _accel_lsbs[] =
{
  1.0f / MPU6500_ACCE_SENS_2,
  1.0f / MPU6500_ACCE_SENS_4,
  1.0f / MPU6500_ACCE_SENS_8,
  1.0f / MPU6500_ACCE_SENS_16,
};

static const float _gyro_lsbs[] = 
{
  1.0f / MPU6500_GYRO_SENS_250,
  1.0f / MPU6500_GYRO_SENS_500,
  1.0f / MPU6500_GYRO_SENS_1000,
  1.0f / MPU6500_GYRO_SENS_2000,
};

////////////////////////////////////////////////////////////////////////////////
//
// private utilities
//
////////////////////////////////////////////////////////////////////////////////
static inline bool
mpu6500_write_reg(mpu6500_t* mpu6500, uint8_t reg, uint8_t data)
{
  mpu6500->num_transactions++;

  if(mpu6500_spi_write(reg, &data, 1) == false)
  {
    mpu6500->num_failed++;
    return false;
  }
  return true;
}

static inline uint8_t
mpu6500_read_reg(mpu6500_t* mpu6500, uint8_t reg)
{
  uint8_t ret;

  mpu6500->num_transactions++;

  if(mpu6500_spi_read(reg, &ret, 1) == true)
  {
    return ret;
  }

  mpu6500->num_failed++;
  return 0;
}

static inline void
mpu6500_read_data(mpu6500_t* mpu6500, uint8_t reg, uint8_t* data, uint8_t len)
{
  mpu6500->num_transactions++;

  if(mpu6500_spi_read(reg, data, len) == false)
  {
    mpu6500->num_failed++;
  }
} 

////////////////////////////////////////////////////////////////////////////////
//
// public utilities
//
////////////////////////////////////////////////////////////////////////////////
void
mpu6500_init(mpu6500_t* mpu6500,
    MPU6500_Accelerometer_t accel_sensitivity,
    MPU6500_Gyroscope_t gyro_sensitivity,
    imu_raw_to_real_t* lsb)
{
  mpu6500_spi_init();

  mpu6500->accel_config       = accel_sensitivity;
  mpu6500->gyro_config        = gyro_sensitivity;
  mpu6500->num_transactions   = 0;
  mpu6500->num_failed         = 0;

  uint8_t temp;

  mpu6500_write_reg(mpu6500, MPU6500_PWR_MGMT_1, 0x00);

  // read who am I
  temp = mpu6500_read_reg(mpu6500, 0x75);
  ESP_LOGI(TAG, "MPU6500 who am I: %x", temp);

  /* Config accelerometer */
  temp = mpu6500_read_reg(mpu6500, MPU6500_ACCEL_CONFIG);
  temp = (temp & 0xE7) | (uint8_t)accel_sensitivity << 3;
  mpu6500_write_reg(mpu6500, MPU6500_ACCEL_CONFIG, temp);

  /* Config gyroscope */
  temp = mpu6500_read_reg(mpu6500, MPU6500_GYRO_CONFIG);
  temp = (temp & 0xE7) | (uint8_t)gyro_sensitivity << 3;
  mpu6500_write_reg(mpu6500, MPU6500_GYRO_CONFIG, temp);

  /* NEW: Enable DLPF for Gyro and Accel (20Hz Bandwidth) */
  // Register 0x1A: bits [2:0] = 100 (0x04) sets 20Hz filter
#if 0
  mpu6500_write_reg(mpu6500, MPU6500_CONFIG, 0x04);
#endif

  // enable bypass mode to access AK8963
  mpu6500_write_reg(mpu6500, 55, 0x02);

  lsb->accel_lsb  = _accel_lsbs[accel_sensitivity];
  lsb->gyro_lsb   = _gyro_lsbs[gyro_sensitivity];

  mpu6500_spi_set_fast(true);
}

bool
mpu6500_read_gyro_accel(mpu6500_t* mpu6500, imu_sensor_data_t* imu)
{
  uint8_t data[14];

  // read full raw data
  mpu6500_read_data(mpu6500, MPU6500_ACCEL_XOUT_H, data, 14);

  imu->accel[0] = (int16_t)(((int16_t)data[0]) << 8 | data[1]);
  imu->accel[1] = (int16_t)(((int16_t)data[2]) << 8 | data[3]);
  imu->accel[2] = (int16_t)(((int16_t)data[4]) << 8 | data[5]);

  imu->temp     = (data[6] << 8 | data[7]);

  imu->gyro[0]  = (int16_t)(((int16_t)data[8]) << 8 | data[9]);
  imu->gyro[1]  = (int16_t)(((int16_t)data[10]) << 8 | data[11]);
  imu->gyro[2]  = (int16_t)(((int16_t)data[12]) << 8 | data[13]);

  return true;
}
