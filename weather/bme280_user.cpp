#include "Arduino.h"
#include "i2c.h"
#include "bme280.h"
#include <stdint.h>

struct bme280_dev dev;
struct bme280_data comp_data;

void
bme280_setup()
{
    int8_t rslt = BME280_OK;

        // set up access to the device and initialize
    dev.dev_id = BME280_I2C_ADDR_PRIM;
    dev.intf = BME280_I2C_INTF;
    dev.read = i2c_reg_read;
    dev.write = i2c_reg_write;
    dev.delay_ms = delay;
    if (rslt = bme280_init(&dev)) {
        Serial.print("bme280_init() failed ");
        Serial.println(rslt);
        return;
    }
        // set the mode of operation
    dev.settings.osr_h = BME280_OVERSAMPLING_1X;
    dev.settings.osr_p = BME280_OVERSAMPLING_16X;
    dev.settings.osr_t = BME280_OVERSAMPLING_2X;
    dev.settings.filter = BME280_FILTER_COEFF_16;
    dev.settings.standby_time = BME280_STANDBY_TIME_1000_MS;

        // start the sampling cycle
    uint8_t settings_sel = BME280_OSR_PRESS_SEL;
    settings_sel |= BME280_OSR_TEMP_SEL;
    settings_sel |= BME280_OSR_HUM_SEL;
    settings_sel |= BME280_STANDBY_SEL;
    settings_sel |= BME280_FILTER_SEL;
    rslt = bme280_set_sensor_settings(settings_sel, &dev);
    if (rslt) {
        Serial.print("bme280_set_sensor_settings() failed ");
        Serial.println(rslt);
        return;
    }

    rslt = bme280_set_sensor_mode(settings_sel, &dev);
    if (rslt) {
        Serial.print("bme280_set_sensor_mode() failed ");
        Serial.println(rslt);
        return;
    }
}
void bme280_read()
{
    int8_t rslt = BME280_OK;
    rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, &dev);
    if (rslt) {
        Serial.print("bme280_get_sensor_data() failed ");
        Serial.println(rslt);
        return;
    }
    /*
    uint8_t data[4];
    i2c_reg_read(BME280_I2C_ADDR_PRIM, 0xF2, data, 4);
    Serial.print(data[0], BIN);
    Serial.print(" ");
    Serial.print(data[1], BIN);
    Serial.print(" ");
    Serial.print(data[2], BIN);
    Serial.print(" ");
    Serial.print(data[3], BIN);
    Serial.print(" ");
    Serial.print("Temp = ");
    Serial.print(comp_data.temperature);
    Serial.print(" Pres = ");
    Serial.print(comp_data.pressure);
    Serial.print(" Humidity = ");
    Serial.println(comp_data.humidity);
*/
}
uint32_t bme280_pressure()
{
    return comp_data.pressure;
}
uint32_t bme280_temperature()
{
    return comp_data.temperature;
}
uint32_t bme280_humidity()
{
    return comp_data.humidity;
}
