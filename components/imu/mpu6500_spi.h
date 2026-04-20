#pragma once

#include <stdint.h>
#include <stdbool.h>

extern void mpu6500_spi_init(void);
extern bool mpu6500_spi_read(uint8_t reg, uint8_t *data, int len);
extern bool mpu6500_spi_write(uint8_t reg, const uint8_t *data, int len);
extern void mpu6500_spi_set_fast(bool fast);
