#include "mpu9250.h"
#include "mpu9250_i2c.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "imu_util.h"

const static char* TAG = "mpu9250";

static const float _accel_lsbs[] =
{
  1.0f / MPU9250_ACCE_SENS_2,
  1.0f / MPU9250_ACCE_SENS_4,
  1.0f / MPU9250_ACCE_SENS_8,
  1.0f / MPU9250_ACCE_SENS_16,
};

static const float _gyro_lsbs[] = 
{
  1.0f / MPU9250_GYRO_SENS_250,
  1.0f / MPU9250_GYRO_SENS_500,
  1.0f / MPU9250_GYRO_SENS_1000,
  1.0f / MPU9250_GYRO_SENS_2000,
};

////////////////////////////////////////////////////////////////////////////////
//
// private utilities
//
////////////////////////////////////////////////////////////////////////////////
static inline bool
mpu9250_write_reg(mpu9250_t* mpu9250, uint8_t reg, uint8_t data)
{
  mpu9250->num_transactions++;

  if(mpu9250_i2c_write(reg, &data, 1) == false)
  {
    mpu9250->num_failed++;
    return false;
  }
  return true;
}

static inline uint8_t
mpu9250_read_reg(mpu9250_t* mpu9250, uint8_t reg)
{
  uint8_t ret;

  mpu9250->num_transactions++;

  if(mpu9250_i2c_read(reg, &ret, 1) == true)
  {
    return ret;
  }

  mpu9250->num_failed++;
  return 0;
}

static inline void
mpu9250_read_data(mpu9250_t* mpu9250, uint8_t reg, uint8_t* data, uint8_t len)
{
  mpu9250->num_transactions++;

  if(mpu9250_i2c_read(reg, data, len) == false)
  {
    mpu9250->num_failed++;
  }
} 

static inline uint8_t
ak8963_read_reg(mpu9250_t* mpu9250, uint8_t reg)
{
  uint8_t ret;

  mpu9250->num_transactions++;

  if(ak8963_i2c_read(reg, &ret, 1) == true)
  {
    return ret;
  }
  mpu9250->num_failed++;
  return 0;
}

static inline void
ak8963_write_reg(mpu9250_t* mpu9250, uint8_t reg, uint8_t data)
{ 
  mpu9250->num_transactions++;

  if(ak8963_i2c_write(reg, &data, 1) == false)
  {
    mpu9250->num_failed++;
  }
}

static inline void
ak8963_read_data(mpu9250_t* mpu9250, uint8_t reg, uint8_t* data, uint8_t len)
{
  mpu9250->num_transactions++;

  if(ak8963_i2c_read(reg, data, len) == false)
  {
    mpu9250->num_failed++;
  }
} 

static void
ak8963_init(mpu9250_t* mpu9250)
{
  uint8_t asa[3];

  // Step 1: Power down
  ak8963_write_reg(mpu9250, 0x0A, 0x00);
  imu_msleep(10);

  // Step 2: Enter Fuse ROM access mode
  ak8963_write_reg(mpu9250, 0x0A, 0x0F);
  imu_msleep(10);

  // Step 3: Read ASA calibration values
  ak8963_read_data(mpu9250, 0x10, asa, 3);
  ESP_LOGI(TAG, "ASA calibration: %02X %02X %02X", asa[0], asa[1], asa[2]);

  // Step 4: Power down again
  ak8963_write_reg(mpu9250, 0x0A, 0x00);
  imu_msleep(10);

  // Step 5: Set to continuous measurement mode 2 (100 Hz, 16-bit output)
  ak8963_write_reg(mpu9250, 0x0A, 0x16);
  imu_msleep(10);
}

static void
ak8963_read_all(mpu9250_t* mpu9250, imu_sensor_data_t* imu)
{
  uint8_t   data[7];

  ak8963_read_data(mpu9250, AK8963_HXL, data, 7);

  imu->mag[0] = (int16_t)(data[1] << 8 | data[0]);
  imu->mag[1] = (int16_t)(data[3] << 8 | data[2]);
  imu->mag[2] = (int16_t)(data[5] << 8 | data[4]);
}

////////////////////////////////////////////////////////////////////////////////
//
// public utilities
//
////////////////////////////////////////////////////////////////////////////////
void
mpu9250_init(mpu9250_t* mpu9250,
    MPU9250_Accelerometer_t accel_sensitivity,
    MPU9250_Gyroscope_t gyro_sensitivity,
    imu_raw_to_real_t* lsb)
{
  mpu9250_i2c_init();

  mpu9250->accel_config       = accel_sensitivity;
  mpu9250->gyro_config        = gyro_sensitivity;
  mpu9250->num_transactions   = 0;
  mpu9250->num_failed         = 0;

  uint8_t temp;

  // dirty special case
  // if cpu resets in the middle of MPU9250 transaction
  // we might have i2c failure at the first write
  // due to stuck SDA line

  /* Wakeup MPU6050 */
  if(mpu9250_write_reg(mpu9250, MPU9250_PWR_MGMT_1, 0x00) == false)
  {
    while(true)
    {
      ESP_LOGE(TAG, "First transaction failed. Retrying...");

      if(mpu9250_write_reg(mpu9250, MPU9250_PWR_MGMT_1, 0x00) == true)
      {
        ESP_LOGI(TAG, "Retried init transaction success!");
        break;
      }
    }
  }

  // read who am I
  temp = mpu9250_read_reg(mpu9250, 0x75);
  ESP_LOGI(TAG, "MPU9250 who am I: %x", temp);

  /* Config accelerometer */
  temp = mpu9250_read_reg(mpu9250, MPU9250_ACCEL_CONFIG);
  temp = (temp & 0xE7) | (uint8_t)accel_sensitivity << 3;
  mpu9250_write_reg(mpu9250, MPU9250_ACCEL_CONFIG, temp);

  /* Config gyroscope */
  temp = mpu9250_read_reg(mpu9250, MPU9250_GYRO_CONFIG);
  temp = (temp & 0xE7) | (uint8_t)gyro_sensitivity << 3;
  mpu9250_write_reg(mpu9250, MPU9250_GYRO_CONFIG, temp);

  /* NEW: Enable DLPF for Gyro and Accel (20Hz Bandwidth) */
  // Register 0x1A: bits [2:0] = 100 (0x04) sets 20Hz filter
#if 0
  mpu9250_write_reg(mpu9250, MPU9250_CONFIG, 0x04);
#endif

  // enable bypass mode to access AK8963
  mpu9250_write_reg(mpu9250, 55, 0x02);

  ak8963_init(mpu9250);

  lsb->accel_lsb  = _accel_lsbs[accel_sensitivity];
  lsb->gyro_lsb   = _gyro_lsbs[gyro_sensitivity];
  lsb->mag_lsb    = AK8963_MAG_LSB;
}

bool
mpu9250_read_gyro_accel(mpu9250_t* mpu9250, imu_sensor_data_t* imu)
{
  uint8_t data[14];

  // read full raw data
  mpu9250_read_data(mpu9250, MPU9250_ACCEL_XOUT_H, data, 14);

  imu->accel[0] = (int16_t)(data[0] << 8 | data[1]);
  imu->accel[1] = (int16_t)(data[2] << 8 | data[3]);
  imu->accel[2] = (int16_t)(data[4] << 8 | data[5]);

  imu->temp     = (data[6] << 8 | data[7]);

  imu->gyro[0]  = (int16_t)(data[8] << 8 | data[9]);
  imu->gyro[1]  = (int16_t)(data[10] << 8 | data[11]);
  imu->gyro[2]  = (int16_t)(data[12] << 8 | data[13]);

  return true;
}

bool
mpu9250_read_mag(mpu9250_t* mpu9250, imu_sensor_data_t* imu)
{
  ak8963_read_all(mpu9250, imu);
  return true;
}

bool
mpu9250_read_all(mpu9250_t* mpu9250, imu_sensor_data_t* imu)
{
  uint8_t data[14];

  // read full raw data
  mpu9250_read_data(mpu9250, MPU9250_ACCEL_XOUT_H, data, 14);

  imu->accel[0] = (int16_t)(data[0] << 8 | data[1]);
  imu->accel[1] = (int16_t)(data[2] << 8 | data[3]);
  imu->accel[2] = (int16_t)(data[4] << 8 | data[5]);

  imu->temp     = (data[6] << 8 | data[7]);

  imu->gyro[0]  = (int16_t)(data[8] << 8 | data[9]);
  imu->gyro[1]  = (int16_t)(data[10] << 8 | data[11]);
  imu->gyro[2]  = (int16_t)(data[12] << 8 | data[13]);

  ak8963_read_all(mpu9250, imu);

  return true;
}
