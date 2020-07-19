#include <Streaming.h>
#include <stdint.h>
#include "i2c.h"
#include "sx1231.h"
#include "bme280_user.h"
#include "ardent.h"

// I2C Devices
#define MPL3115_ADDRESS 0x60

// Feather M0 IO Ports
#define RED_LED 13
#define BLUE_LED 10
#define GREEN_LED 6

#define RADIO_CS 8
#define RADIO_INT 3
#define RADIO_RESET 4

    // Analog ports
#define BATTERY 7

    // samples collected per transmission
#define TX_SAMPLES 1

unsigned MPL3115_temperature;   // Celceus
unsigned MPL3115_pressure;      // Pascals
unsigned humidity;
unsigned ardent_wind_count;      // kpH
unsigned ardent_direction;      // angle
unsigned ardent_rainfall;
unsigned battery_voltage;
unsigned uptime;

uint32_t previous_wind_count;
uint8_t wind_speed_history[TX_SAMPLES];
uint8_t wind_dir_history[TX_SAMPLES];

uint32_t minute_start_time;     // start time for 1 minute collection interval in milliseconds
uint32_t next_sample_time;

// routine to create weather info packet for transmission
uint32_t
weather_pack(uint32_t value, char *array, uint32_t length)
{
    unsigned size = length;
    while (length--) {
        array[length] = value & 0xFF;
        value >>=8;
    }
    return size;
}

// the setup function runs once when you press reset or power the board
void setup() {
    // The USB serial interface takes several seconds to open if the board is connected to the system and if the
    // board was loaded/reset via the Arduino tool
    int count=0;
    while (count<1000 && !Serial) { // only wait 10 seconds in case board is running standalone
        count++;
        delay(1);
    }
    char buffer[100];
    sprintf(buffer, "Startup %d\n", count);
    Serial.print(buffer);
    
    // initialize digital pins
    pinMode(RED_LED, OUTPUT);
    pinMode(BLUE_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);
    pinMode(ARDENT_RAIN, INPUT_PULLUP);
    pinMode(ARDENT_SPEED, INPUT_PULLUP);

    // initialize the I2C interface
    i2c_init();

    // initialize the MPL3115A2 pressure/temperature sensor
    unsigned i;
    uint8_t message[] = {
        0x26,0x38,  // pressure, 128x oversample
        0x27,0x02, // 4 second interval
        0x13,0x07, // enable update flags

        0x26,0x39  // pressure, 128x oversampling, enable u=uto polling
    };
    for (i=0; i<8; i+=2) {  
        i2c_reg_write(MPL3115_ADDRESS, message[i], message+i+1, 1);
    }

    // initialize the BME280 pressure/temperature/humidity sensor
    bme280_setup();

    // initialize the radio
    radioInit(RADIO_CS, RADIO_RESET, RADIO_INT);

    // initialize the wind and rain subsystem
    ardentInit();

    // turn off the red LED
    digitalWrite(RED_LED, LOW);    // turn the LED off by making the voltage LOW

//  REG_USB_CTRLA &= ~USB_CTRLA_ENABLE;
//  Serial.println(REG_USB_CTRLA, HEX);

    // Arduino library sets up the ADC as follows:
    // REFCTRL.REFSEL = 2   // 1/2 VDDANA
    // AVGCTRL.SAMPLENUM = 0    // one sample
    // SAMPCTRL.SAMPLEN = 5     // 3 ADC clocks sample time
    // CTRLB.PRESCALER = 3      // peripheral clock divided by 128
    // CTRLB.RESSEL = 0         // 12-bit resolution
    // CTRLB.FREERUN = 0        // Single conversion
    // INPUTCTRL.GAIN = F       // divide by 2
    // INPUTCTRL.MUXNEG = 18    // GND
    // INPUTCTRL.MUXPOS = 0     // Analog input 0

    battery_voltage = analogRead(7);
    Serial.print("Adc->CTRLA.reg = ");
    Serial.println(ADC->CTRLA.reg, HEX);
    Serial.print("Adc->REFCTRL.reg = ");
    Serial.println(ADC->REFCTRL.reg, HEX);
    Serial.print("Adc->AVGCTRL.reg = ");
    Serial.println(ADC->AVGCTRL.reg, HEX);
    Serial.print("Adc->SAMPCTRL.reg = ");
    Serial.println(ADC->SAMPCTRL.reg, HEX);
    Serial.print("Adc->CTRLB.reg = ");
    Serial.println(ADC->CTRLB.reg, HEX);
    Serial.print("Adc->WINCTRL.reg = ");
    Serial.println(ADC->WINCTRL.reg, HEX);
    Serial.print("Adc->INPUTCTRL.reg = ");
    Serial.println(ADC->INPUTCTRL.reg, HEX);
    Serial.print("Adc->STATUS.reg = ");
    Serial.println(ADC->STATUS.reg, HEX);
    Serial.print("Adc->RESULT.reg = ");
    Serial.println(ADC->RESULT.reg, HEX);

    minute_start_time = millis();
    next_sample_time = millis();
    previous_wind_count = ardent_wind_count;

}

// the loop function runs over and over again forever
void loop() {
/*
    digitalWrite(RED_LED, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(100);              // wait for a second

    digitalWrite(RED_LED, LOW);    // turn the LED off by making the voltage LOW

    digitalWrite(BLUE_LED, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(100);              // wait for a second
    digitalWrite(BLUE_LED, LOW);    // turn the LED off by making the voltage LOW

    digitalWrite(GREEN_LED, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(100);              // wait for a second
    digitalWrite(GREEN_LED, LOW);    // turn the LED off by making the voltage LOW

    delay(900);              // wait for a second
*/
    int i;
    for (i=0; i<TX_SAMPLES; i++) {
//      next_start += 5000;
//      uint32_t next_delay = next_start - millis();
        next_sample_time += 5000;
        uint32_t next_delay = next_sample_time - millis();
        Serial.println(next_delay);
        if (next_delay < 10000)
            delay(next_delay);
        uint32_t current_wind_count = ardent_wind_count;
        wind_speed_history[i] = current_wind_count - previous_wind_count;
        previous_wind_count = current_wind_count;
    }
    
    // Check the pressure/temperature status
    char buffer[80];
    uint8_t status;
    i2c_reg_read(MPL3115_ADDRESS, 0, &status, 1);
    if (status!=0) {
        unsigned value, fraction;
        uint8_t read_data[6];
        memset(read_data, 0, 6);
        if (status & 0x04) {
            i2c_reg_read(MPL3115_ADDRESS, 1, read_data+1, 1);
            i2c_reg_read(MPL3115_ADDRESS, 2, read_data+2, 1);
            i2c_reg_read(MPL3115_ADDRESS, 3, read_data+3, 1);
            value = (read_data[1]<<16 | read_data[2]<<8 | read_data[3])>>4;
            MPL3115_pressure = (value >> 2);
            int in_hg_hundreds = (MPL3115_pressure*100) / 3386;
            int in_hg = in_hg_hundreds/100;
            int in_hg_fraction = in_hg_hundreds % 100;
            sprintf(buffer, "Pressure = %d.%02d\n", in_hg, in_hg_fraction);
            Serial.print(buffer);
        }
        if (status & 0x02) {
            i2c_reg_read(MPL3115_ADDRESS, 4, read_data+4, 1);
            i2c_reg_read(MPL3115_ADDRESS, 5, read_data+5, 1);
            value = (read_data[4]<<8 | read_data[5]) >> 4;
            MPL3115_temperature = read_data[4];
            fraction = read_data[5] * 390625;
            sprintf(buffer, "Temperature = %d.%04d\n", MPL3115_temperature, fraction);
            Serial.print(buffer);
        }
    }
    battery_voltage = analogRead(7);
    bme280_read();
    ardent_dir();
//  uint32_t next_start = minute_start_time;

//  minute_start_time += 60000;
/*
    // format the output for transmission
    sprintf(buffer, "T=%d;P=%d;H=%d;S=%d;D=%d;R=%d;B=%d",
        MPL3115_temperature,
        MPL3115_pressure,
        humidity,
        ardent_wind_count,
        ardent_direction,
        ardent_rainfall,
        battery_voltage
        );
    sendFrame(buffer, strlen(buffer));

    delay(1000);
*/
    buffer[0] = 'A';
    char *p;
    p = buffer + 1;
    p += weather_pack(battery_voltage, p, 2);
    p += weather_pack(MPL3115_temperature, p, 4);
    p += weather_pack(MPL3115_pressure, p, 4);
    p += weather_pack(bme280_temperature(), p, 4);
    p += weather_pack(bme280_pressure(), p, 4);
    p += weather_pack(bme280_humidity(), p, 4);
    p += weather_pack(uptime, p, 4);
    p += weather_pack(ardent_direction, p, 4);
    p += weather_pack(ardent_wind_count, p, 4);
    p += weather_pack(ardent_rainfall, p, 4);
    for (i=0; i<TX_SAMPLES; i++)
        p += weather_pack(wind_speed_history[i], p, 1);
    sendFrame(buffer, p-buffer);
    uptime++;

    digitalWrite(GREEN_LED, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(100);              // wait for a second
    digitalWrite(GREEN_LED, LOW);    // turn the LED off by making the voltage LOW
    
}
