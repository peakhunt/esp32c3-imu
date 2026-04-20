#pragma once

#include <stdint.h>
#include <stdbool.h>

// HMC5983 at +/- 1.3 Gauss (Default) = 1090 LSB/Gauss
// 1 Gauss = 100 uT. So 100 / 1090 = 0.0917 uT per LSB
#define HMC5983_MAG_LSB               0.0917f 

typedef struct hmc5983Mag_s
{
  uint64_t            num_transactions;
  uint64_t            num_failed;
} hmc5983Mag;

extern void hmc5983_init(hmc5983Mag* mag, float* lsb);
extern void hmc5983_read(hmc5983Mag* mag, int16_t m[3]);
