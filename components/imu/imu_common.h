#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
  int16_t   accel[3];
  int16_t   gyro[3];
  int16_t   mag[3];
  int16_t   temp;
} imu_sensor_data_t;

typedef struct
{
  float accel_lsb;
  float gyro_lsb;
  float mag_lsb;
} imu_raw_to_real_t;
