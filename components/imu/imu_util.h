#pragma once

#include "freertos/FreeRTOS.h"
#include "esp_rom_sys.h"

static inline void
imu_msleep(int msec)
{
  // we don't want task switching because IMU initialization
  // must be done before anything else and
  // this is only called during initialization phase.
  //
  // vTaskDelay(pdMS_TO_TICKS(msec));
  esp_rom_delay_us(msec * 1000);
}

#define IMU_MSEC_TO_TICKS(msec)   pdMS_TO_TICKS(msec)
