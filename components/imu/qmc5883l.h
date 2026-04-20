#pragma once

#include <stdint.h>
#include <stdbool.h>

#define QMC5883L_MAG_LSB              0.0333f     // +-8 Gauss, 0.0333uT per LSB
//#define QMC5883L_MAG_LSB              0.0083f     // +-2 Gauss, 0.0083uT per LSB

typedef struct qmc5883lMag_s
{
  uint64_t            num_transactions;
  uint64_t            num_failed;
} qmc5883lMag;

extern void qmc5883l_init(qmc5883lMag* mag, float* lsb);
extern void qmc5883l_read(qmc5883lMag* mag, int16_t m[3]);
