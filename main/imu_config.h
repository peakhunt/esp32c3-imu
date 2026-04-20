#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "imu.h"

#define IMU_CONFIG_MAGIC        0xdeadbeef
#define IMU_CONFIG_REVISION     1

typedef struct
{
  // --- STATION MODE (DHCP ONLY) ---
  bool      sta_enabled;
  char      sta_ssid[32];
  char      sta_password[64];

  // --- AP MODE (STATIC) ---
  char      ap_ssid[32];
  char      ap_password[64];
  char      ap_ip[16];      // e.g. "192.168.4.1"
  char      ap_mask[16];    // e.g. "255.255.255.0"
  uint8_t   channel;
} imu_wifi_config_t;

typedef struct _imu_config_t
{
  uint32_t                  magic;
  uint16_t                  revision;

  // The Data Blocks
  imu_sensor_calib_data_t   sensor; // Your existing calib
  imu_engine_config_t       engine; // AHRS Params
  imu_wifi_config_t         wifi;   // Network Params

} imu_config_t;

extern void imu_config_init(void);
extern void imu_config_update_accel_calib(int16_t off[3], int16_t scale[3]);
extern void imu_config_update_gyro_calib(int16_t off[3]);
extern void imu_config_update_mag_calib(int16_t bias[3], int16_t scale[3]);
extern void imu_config_update_ahrs(imu_engine_config_t* cfg);
extern void imu_config_get_sensor_config(imu_sensor_calib_data_t* cfg);
extern void imu_config_get_imu_engine_config(imu_engine_config_t* cfg);
extern void imu_config_get_wifi_config(imu_wifi_config_t* cfg);
extern void imu_config_update_wifi_config(imu_wifi_config_t* cfg);
