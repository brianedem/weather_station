#include <stdint.h>
#ifndef I2C_H
#define I2C_H

void i2c_init();
int8_t i2c_reg_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *buffer, uint16_t length);
int8_t i2c_reg_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *buffer, uint16_t length);

#endif
