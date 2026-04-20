#pragma once

#include "imu.h"

typedef void (*imu_task_calib_complete_cb)(void*);

extern void imu_task_init(void);
extern void imu_task_get_data(imu_data_t* data);
extern void imu_task_get_attitude(float* roll, float* pitch, float* yaw);

extern void imu_task_start_gyro_calibration(imu_task_calib_complete_cb cb, void* data);
extern void imu_task_get_gyro_calibration(float offset[3]);
extern void imu_task_start_mag_calibration(imu_task_calib_complete_cb cb, void* data);
extern void imu_task_get_mag_calibration(float bias[3], float scale[3]);

extern void imu_task_start_accel_calibration(bool reset, imu_task_calib_complete_cb cb, void* data);
extern bool imu_task_finish_accel_calibration(void);
extern void imu_task_get_accel_calibration(float offset[3], float scale[3]);

extern void imu_task_config_ahrs(imu_engine_config_t* cfg);

extern volatile uint32_t _sample_rate;
