#ifndef __MPU9250_I2C_DEF_H__
#define __MPU9250_I2C_DEF_H__

#include <stdint.h>
#include <stdbool.h>

#define MPU9250_I2C_NUM                 I2C_NUM_0
#define MPU9250_I2C_MASTER_SDA_IO       19
#define MPU9250_I2C_MASTER_SCL_IO       18
#define MPU9250_I2C_MASTER_NCS_IO       20
#define MPU9250_I2C_CLK_FREQ            400000

extern void mpu9250_i2c_init(void);
extern bool mpu9250_i2c_read(uint8_t reg, uint8_t *data, int len);
extern bool mpu9250_i2c_write(uint8_t reg, const uint8_t *data, int len);
extern bool ak8963_i2c_read(uint8_t reg, uint8_t *data, int len);
extern bool ak8963_i2c_write(uint8_t reg, const uint8_t *data, int len);

#endif /* !__MPU9250_I2C_DEF_H__ */
