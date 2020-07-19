#include "i2c.h"
#include "Arduino.h"
#include <Wire.h>
void
i2c_init()
{
    Wire.begin();
}

int8_t
i2c_reg_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *buffer, uint16_t length)
{
    Wire.beginTransmission(dev_addr);
    Wire.write(reg_addr);
    Wire.write(buffer, length);
    return Wire.endTransmission(1);
}

int8_t
i2c_reg_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *buffer, uint16_t length)
{
    Wire.beginTransmission(dev_addr);
    Wire.write(reg_addr);
    unsigned status = Wire.endTransmission(0);
    if (status!=0) {
        Serial.print(status, HEX);
        Serial.print(' ');
        Serial.print(dev_addr, HEX);
        Serial.print(' ');
        Serial.print(reg_addr, HEX);
        Serial.print(' ');
        Serial.println(length);
    }
    Wire.requestFrom(dev_addr, length);
    while (length--)
        *buffer++ = Wire.read();
    return 0;
}


