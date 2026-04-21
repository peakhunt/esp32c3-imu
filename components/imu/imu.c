#include <stdio.h>
#include <string.h>
#include <math.h>
#include "imu.h"
#include "gyro_calibration.h"
#include "mag_calibration.h"
#include "accel_calibration.h"

///////////////////////////////////////////////////////////////////////////////
//
// Sensor Reading(I know it's very confusing. Be sure to read/understand this)
// 
// Accelerometer
// It's an opposite force of Gravity
// So in NED convention, when lying still,
// When Z axis points downward(toward Gravity), Z should report -1g
// When Y axis points downward, Y should report -1g
// When X axis points downward, X should report -1g
//
// Gyro
// Right hand rule in NED convention
// With your thumb points positive axis direction,
// direction oo your finger curl is positive rotation value
// Rotation around X(N) : Right wing down should indicate positive Gyro X
// Rotation around Y(E) : Nose up should indicate positive Gyro Y
// Rotation around Z(D) : Rotation clockwise should indicate positive Gyro Z
//
// Magnetometer
// Basically (I said basically) when each axis points to magnetic north
// You should see the max reading
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
// utilities
//
////////////////////////////////////////////////////////////////////////////////
static inline void
alignReading(int16_t* values, imu_sensor_align_t align)
{
  const int16_t   x = values[0],
                  y = values[1],
                  z = values[2];

  switch(align)
  {
  case imu_sensor_align_cw_0:
    values[0] = x;
    values[1] = y;
    values[2] = z;
    break;

  case imu_sensor_align_cw_90:
    values[0] = y;
    values[1] = -x;
    values[2] = z;
    break;

  case imu_sensor_align_cw_180:
    values[0] = -x;
    values[1] = -y;
    values[2] = z;
    break;

  case imu_sensor_align_cw_270:
    values[0] = -y;
    values[1] = x;
    values[2] = z;
    break;

  case imu_sensor_align_cw_0_flip:
    values[0] = -x;
    values[1] = y;
    values[2] = -z;
    break;

  case imu_sensor_align_cw_90_flip:
    values[0] = y;
    values[1] = x;
    values[2] = -z;
    break;

  case imu_sensor_align_cw_180_flip:
    values[0] = x;
    values[1] = -y;
    values[2] = -z;
    break;

  case imu_sensor_align_cw_270_flip:
    values[0] = -y;
    values[1] = -x;
    values[2] = -z;
    break;

  case imu_sensor_align_special:
    values[0] = -y;
    values[1] = -x;
    values[2] =  z;
    break;

  case imu_sensor_align_special2:
    values[0] = x;
    values[1] = y;
    values[2] = -z;
    break;
  }
}

static void
imu_apply_calibration(imu_t* imu)
{
  imu->adjusted.accel[0] = (imu->raw.accel[0] - imu->cal.accel_off[0]) * imu->cal.accel_scale[0] / 4096;
  imu->adjusted.accel[1] = (imu->raw.accel[1] - imu->cal.accel_off[1]) * imu->cal.accel_scale[1] / 4096;
  imu->adjusted.accel[2] = (imu->raw.accel[2] - imu->cal.accel_off[2]) * imu->cal.accel_scale[2] / 4096;

  imu->adjusted.gyro[0] = (imu->raw.gyro[0] - imu->cal.gyro_off[0]);
  imu->adjusted.gyro[1] = (imu->raw.gyro[1] - imu->cal.gyro_off[1]);
  imu->adjusted.gyro[2] = (imu->raw.gyro[2] - imu->cal.gyro_off[2]);

#if IMU_USE_MAG_CALIB_SOFT_IRON == 0
  imu->adjusted.mag[0] = (imu->raw.mag[0] - imu->cal.mag_bias[0]);
  imu->adjusted.mag[1] = (imu->raw.mag[1] - imu->cal.mag_bias[1]);
  imu->adjusted.mag[2] = (imu->raw.mag[2] - imu->cal.mag_bias[2]);
#else
  imu->adjusted.mag[0] = (imu->raw.mag[0] - imu->cal.mag_bias[0]) * imu->cal.mag_scale[0] / 4096;
  imu->adjusted.mag[1] = (imu->raw.mag[1] - imu->cal.mag_bias[1]) * imu->cal.mag_scale[1] / 4096;
  imu->adjusted.mag[2] = (imu->raw.mag[2] - imu->cal.mag_bias[2]) * imu->cal.mag_scale[2] / 4096;
#endif

  imu->adjusted.temp = imu->raw.temp;
}

static void
imu_apply_sensor_orientation(imu_t* imu)
{
  alignReading(imu->raw.accel, imu->accel_align);
  alignReading(imu->raw.gyro, imu->gyro_align);
  alignReading(imu->raw.mag, imu->mag_align);
}

static void
imu_calc_sensor_value(imu_t* imu)
{
  imu->data.accel[0] = imu->adjusted.accel[0] * imu->lsb.accel_lsb;
  imu->data.accel[1] = imu->adjusted.accel[1] * imu->lsb.accel_lsb;
  imu->data.accel[2] = imu->adjusted.accel[2] * imu->lsb.accel_lsb;

  imu->data.gyro[0] = imu->adjusted.gyro[0] * imu->lsb.gyro_lsb;
  imu->data.gyro[1] = imu->adjusted.gyro[1] * imu->lsb.gyro_lsb;
  imu->data.gyro[2] = imu->adjusted.gyro[2] * imu->lsb.gyro_lsb;

  imu->data.mag[0] = imu->adjusted.mag[0] * imu->lsb.mag_lsb;
  imu->data.mag[1] = imu->adjusted.mag[1] * imu->lsb.mag_lsb;
  imu->data.mag[2] = imu->adjusted.mag[2] * imu->lsb.mag_lsb;

#if IMU_STORE_UNCALIBRATED_DATA == 1
  imu->data.u_accel[0] = imu->raw.accel[0] * imu->lsb.accel_lsb;
  imu->data.u_accel[1] = imu->raw.accel[1] * imu->lsb.accel_lsb;
  imu->data.u_accel[2] = imu->raw.accel[2] * imu->lsb.accel_lsb;

  imu->data.u_gyro[0] = imu->raw.gyro[0] * imu->lsb.gyro_lsb;
  imu->data.u_gyro[1] = imu->raw.gyro[1] * imu->lsb.gyro_lsb;
  imu->data.u_gyro[2] = imu->raw.gyro[2] * imu->lsb.gyro_lsb;

  imu->data.u_mag[0] = imu->raw.mag[0] * imu->lsb.mag_lsb;
  imu->data.u_mag[1] = imu->raw.mag[1] * imu->lsb.mag_lsb;
  imu->data.u_mag[2] = imu->raw.mag[2] * imu->lsb.mag_lsb;
#endif

  imu->data.temp = ((float) imu->adjusted.temp) / 333.87f + 21.0f;
}

static void
imu_update_normal(imu_t* imu)
{
  imu_apply_sensor_orientation(imu);
  imu_apply_calibration(imu);
  imu_calc_sensor_value(imu);

  if(imu->engine_cfg.ahrs_mode == IMU_AHRS_MODE_MADGWICK)
  {
    madgwick_update(&imu->filter_madgwick,
        imu->data.gyro[0],  imu->data.gyro[1],   imu->data.gyro[2],
        imu->data.accel[0], imu->data.accel[1],  imu->data.accel[2],
        imu->data.mag[0],   imu->data.mag[1],    imu->data.mag[2]);

    madgwick_get_roll_pitch_yaw(&imu->filter_madgwick,
        imu->data.orientation,
        imu->engine_cfg.mag_declination);
  }
  else
  {
    mahony_update(&imu->filter_mahony,
        imu->data.gyro[0],  imu->data.gyro[1],   imu->data.gyro[2],
        imu->data.accel[0], imu->data.accel[1],  imu->data.accel[2],
        imu->data.mag[0],   imu->data.mag[1],    imu->data.mag[2]);

    mahony_get_roll_pitch_yaw(&imu->filter_mahony,
        imu->data.orientation,
        imu->engine_cfg.mag_declination);
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// public interfaces
//
////////////////////////////////////////////////////////////////////////////////
void
imu_init(imu_t* imu, imu_engine_config_t* cfg, float hz,
    imu_sensor_align_t a_align,
    imu_sensor_align_t g_align,
    imu_sensor_align_t m_align)
{
  memset(imu, 0, sizeof(imu_t));

  imu->mode = imu_mode_normal;
  memcpy(&imu->engine_cfg, cfg, sizeof(imu_engine_config_t));

  imu->cal.accel_scale[0] = 
  imu->cal.accel_scale[1] = 
  imu->cal.accel_scale[2] = 4096;

  imu->cal.mag_scale[0] = 
  imu->cal.mag_scale[1] = 
  imu->cal.mag_scale[2] = 4096;

  imu->accel_align  = a_align;
  imu->gyro_align   = g_align; 
  imu->mag_align    = m_align;

  imu->update_rate  = hz;

  madgwick_init(&imu->filter_madgwick, imu->update_rate, cfg->madgwick_beta);
  mahony_init(&imu->filter_mahony, imu->update_rate, cfg->mahony_kp, cfg->mahony_ki);
}

void
imu_update(imu_t* imu)
{
  // raw sensor data is already read in .raw
  //
  imu_update_normal(imu);
  //
  // XXX
  // by above function call,
  // ssensor inputs in raw are already algined in NED right hand rule axis convention
  // so all the sensor calibration is done in NED right hand axis convention
  // this can be quite confusing and lead to a big subtle bug 
  // if you don't pay attention
  //
  switch(imu->mode)
  {
  case imu_mode_normal:
    break;

  case imu_mode_accel_calibrating:
    accel_calibration_update(imu->raw.accel[0], imu->raw.accel[1], imu->raw.accel[2]);
    break;

  case imu_mode_gyro_calibrating:
    gyro_calibration_update(imu->raw.gyro[0], imu->raw.gyro[1], imu->raw.gyro[2]);
    break;

  case imu_mode_mag_calibrating:
    mag_calibration_update(imu->raw.mag[0], imu->raw.mag[1], imu->raw.mag[2]);
    break;
  }
}

void
imu_gyro_calibration_start(imu_t* imu)
{
  imu->mode = imu_mode_gyro_calibrating;
  gyro_calibration_init();
}

void
imu_gyro_calibration_finish(imu_t* imu)
{
  imu->mode = imu_mode_normal;
  gyro_calibration_finish(imu->cal.gyro_off);
}

void
imu_gyro_get_calibration(imu_t* imu, float data[3])
{
  data[0] = imu->cal.gyro_off[0] * imu->lsb.gyro_lsb;
  data[1] = imu->cal.gyro_off[1] * imu->lsb.gyro_lsb;
  data[2] = imu->cal.gyro_off[2] * imu->lsb.gyro_lsb;
}

void
imu_mag_calibration_start(imu_t* imu)
{
  imu->mode = imu_mode_mag_calibrating;
  mag_calibration_init();
}

void
imu_mag_calibration_finish(imu_t* imu)
{
  imu->mode = imu_mode_normal;
#if IMU_USE_MAG_CALIB_SOFT_IRON == 0
  mag_calibration_finish(imu->cal.mag_bias);
#else
  mag_calibration_finish_with_soft_iron(imu->cal.mag_bias, imu->cal.mag_scale);
#endif
}

void
imu_mag_get_calibration(imu_t* imu, float bias[3], float scale[3])
{
  bias[0] = imu->cal.mag_bias[0] * imu->lsb.mag_lsb;
  bias[1] = imu->cal.mag_bias[1] * imu->lsb.mag_lsb;
  bias[2] = imu->cal.mag_bias[2] * imu->lsb.mag_lsb;

  // 2. Soft Iron Scale (Convert 4096-base int to 1.0-base float)
  scale[0] = imu->cal.mag_scale[0] / 4096.0f;
  scale[1] = imu->cal.mag_scale[1] / 4096.0f;
  scale[2] = imu->cal.mag_scale[2] / 4096.0f;
}

void
imu_accel_calibration_init(imu_t* imu)
{
  accel_calibration_init();
}

void
imu_accel_calibration_step_start(imu_t* imu)
{
  imu->mode = imu_mode_accel_calibrating;
}

void
imu_accel_calibration_step_stop(imu_t* imu)
{
  imu->mode = imu_mode_normal;
}

bool
imu_accel_calibration_finish(imu_t* imu)
{
  imu->mode = imu_mode_normal;
  return accel_calibration_finish(imu->cal.accel_off, imu->cal.accel_scale);
}

void
imu_accel_get_calibration(imu_t* imu, float off[3], float scale[3])
{
  off[0] = imu->cal.accel_off[0] * imu->lsb.accel_lsb;
  off[1] = imu->cal.accel_off[1] * imu->lsb.accel_lsb;
  off[2] = imu->cal.accel_off[2] * imu->lsb.accel_lsb;

  scale[0] = imu->cal.accel_scale[0] / 4096.0f;
  scale[1] = imu->cal.accel_scale[1] / 4096.0f;
  scale[2] = imu->cal.accel_scale[2] / 4096.0f;
}

void
imu_config_engine(imu_t* imu, imu_engine_config_t* cfg)
{
  memcpy(&imu->engine_cfg, cfg, sizeof(imu_engine_config_t));
  madgwick_init(&imu->filter_madgwick, imu->update_rate, cfg->madgwick_beta);
  mahony_init(&imu->filter_mahony, imu->update_rate, cfg->mahony_kp, cfg->mahony_ki);
}
