#include "bme280.h"
#include <stdint.h>

void bme280_setup();
void bme280_read();
uint32_t bme280_pressure();
int32_t bme280_temperature();
uint32_t bme280_humidity();
