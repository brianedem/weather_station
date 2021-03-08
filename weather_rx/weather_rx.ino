#include <Streaming.h>
#include <stdint.h>
#include "sx1231.h"
#include <SPI.h>

#define ELEVATION_CORRECTION 910    // in pascals

// Feather M0 IO Ports
#define RED_LED LED_BUILTIN // 13

#ifdef _VARIANT_FEATHER52832_
#define RADIO_CS    11
#define RADIO_RESET 7
#define DIO0        2
#define DIO1        3
#define DIO2        4
#define DIO3        5
#define DIO4        28
#define DIO5        29

#else
#define RADIO_CS    8
#define RADIO_RESET 4
#define DIO0        3
#endif

#define     RADIO_INT   DIO0

void
writeRadio(byte reg_addr, byte value)
{
    digitalWrite(RADIO_CS, LOW);
    SPI.transfer(reg_addr | 0x80);
    SPI.transfer(value);
    digitalWrite(RADIO_CS, HIGH);
}

uint32_t
readRadio(byte reg_addr)
{
    digitalWrite(RADIO_CS, LOW);
    SPI.transfer(reg_addr | 0x00);
    byte result = SPI.transfer(0x00);
    digitalWrite(RADIO_CS, HIGH);
    return result;
}

uint32_t
radioSpi(uint32_t operation, uint32_t reg_addr, uint32_t writeValue, uint32_t bytes)
{
    uint32_t result = 0;
    digitalWrite(RADIO_CS, LOW);
    SPI.transfer(reg_addr | operation);
    while (bytes--)
         result = result<<8 | SPI.transfer(writeValue>>(bytes*8));
    digitalWrite(RADIO_CS, HIGH);
    return result;
}

uint32_t
readRadioIntr() {
    return digitalRead(RADIO_INT);
}

uint32_t led_state = HIGH;

// the setup function runs once when you press reset or power the board
void setup() {

    char buffer[100];
    uint32_t count=0;

#ifdef _VARIANT_FEATHER52832_
    Serial.begin(115200);
#else
    // The USB serial interface takes several seconds to open if the board
    // is connected to the system and if the board was loaded/reset via
    // the Arduino tool
    while (count<1000 && !Serial) { // only wait 10 seconds in case board is running standalone
        count++;
        delay(1);
    }
#endif

    sprintf(buffer, "I:Startup %d\n", count);
    Serial.print(buffer);
    
    // initialize digital pins
    pinMode(RED_LED, OUTPUT);

        // set up the radio
    pinMode(RADIO_INT, INPUT);
    pinMode(RADIO_CS, OUTPUT);
    pinMode(RADIO_RESET, OUTPUT);
    digitalWrite(RADIO_CS, HIGH);

        // initialize the SPI interface to the radio
    SPI.begin();

        // reset the radio
    digitalWrite(RADIO_RESET, HIGH);
    delay(10);      // assert for a minimum of 100us
    digitalWrite(RADIO_RESET, LOW);
    delay(10);      // wait at least 5ms; but 10ms after POR

        // initialize the radio
    radioInit();
}

// routine to extract weather packet elements
// multi-byte elements are transmitted MSB first
uint32_t
weather_unpack(uint8_t *m, uint32_t size)
{
    uint32_t value = 0;
    while (size--)
        value = value<<8 | *m++;
    return value;
}

uint32_t previous_uptime = 0;
uint32_t packet_count = 1;
uint32_t packet_loss = 0;

extern uint32_t rssi;
extern uint32_t rxFrequency;
extern int16_t fei;
extern int16_t afc;


// the loop function runs over and over again forever
void loop() {

    char buffer[200];
    char wind_buffer[10];
    uint8_t message[64];
    uint8_t *m = message+2;
    uint32_t count = receiveFrame(message);
    uint32_t length = message[0]-1;
//  sprintf(buffer, "D:length = %d", length);
//  Serial.println(buffer);
//  sprintf(buffer, "D:%d bytes read from FIFO\n", count);
//  Serial.print(buffer);
    char message_type = message[1];
    if (length!=length) {
        sprintf(buffer, "E:message length mismatch");
        Serial.println(buffer);
    }
    else if (message_type == 'A') {
        uint32_t battery_voltage        = weather_unpack(m+ 0, 2);
        uint32_t MPL3115_temperature    = weather_unpack(m+ 2, 4);
        uint32_t MPL3115_pressure       = weather_unpack(m+ 6, 4) + ELEVATION_CORRECTION;       // pascals
        uint32_t BME280_temperature     = weather_unpack(m+10, 4);
        uint32_t BME280_pressure        = weather_unpack(m+14, 4) + ELEVATION_CORRECTION*100;   // pascals*100
        uint32_t BME280_humidity        = weather_unpack(m+18, 4);
        uint32_t uptime                 = weather_unpack(m+22, 4);
        uint32_t ardent_direction       = weather_unpack(m+26, 4);
        uint32_t ardent_windspeed       = weather_unpack(m+30, 4);
        uint32_t ardent_rainfall        = weather_unpack(m+34, 4);
        uint8_t windspeed[12];

        uint32_t i;
        uint32_t wind_count;
        for (i=38,wind_count=0; i<length; i++,wind_count++) {
            windspeed[wind_count] = m[i];
        }
        if (1) {
            sprintf(buffer, "M:%d %d %d %d %d %d %d %d %d %d %d %d %d %d",
                rssi,
                rxFrequency,
                fei,
                afc,
                battery_voltage,
                MPL3115_temperature,
                MPL3115_pressure,
                BME280_temperature,
                BME280_pressure,
                BME280_humidity,
                uptime,
                ardent_direction,
                ardent_windspeed,
                ardent_rainfall);
            for (i=0; i<wind_count; i++) {
                sprintf(wind_buffer, " %d", windspeed[i]);
                strcat(buffer, wind_buffer);
            }
            Serial.println(buffer);
            
        }
        else {
                // need to adjust voltage by 3.3/2048
            battery_voltage *= 3;
            battery_voltage += battery_voltage/10;
            sprintf(buffer, "D:System:  %d, battery = %d.%03d, uptime = %u\n",
                millis()/1000, battery_voltage/2048, ((battery_voltage&0x7FF)*1000)>>11, uptime);
            Serial.print(buffer);
            sprintf(buffer, "D:Ardent:  direction = %d, rainfall = %d, wind_count = %d\n",
                ardent_direction, ardent_rainfall, ardent_windspeed);
            Serial.print(buffer);
            sprintf(buffer, "D:MLP3115: temperature = %d, pressure = %d\n",
                MPL3115_temperature, MPL3115_pressure/100);
            Serial.print(buffer);
            sprintf(buffer, "D:BME280_temperature = %d.%02d, pressure = %d.%02d, humidity = %d.%1d\n",
                BME280_temperature/100, BME280_temperature%100,
                BME280_pressure/10000, (BME280_pressure/100)%100,
                BME280_humidity/1024, (BME280_humidity*10/1024)%10);
            Serial.print(buffer);
            char *b = buffer + sprintf(buffer, "D:5 second windspeed:");
            for (i=38; i<length; i++) {
                unsigned windspeed_mph = (weather_unpack(m+i, 1) * 1492)/1000/5;
                b += sprintf (b, " %2d", windspeed_mph);
            }
            sprintf(b, "\n");
            Serial.print(buffer);

            if (previous_uptime==0)
                previous_uptime = uptime;
            else {
                packet_count += uptime - previous_uptime++;
                packet_loss  += uptime - previous_uptime;
                previous_uptime = uptime;
            }
            sprintf(buffer, "D:%d packets received, %d packets lost, %d loss rate\n",
                packet_count, packet_loss, packet_loss*100/packet_count);
            Serial.print(buffer);
        }
    }
    else {
        sprintf(buffer, "E:%d bytes read from FIFO\n", count);
        Serial.print(buffer);
        sprintf(buffer, "E:unknown message type %c\n", message_type);
        Serial.print(buffer);
    }

        // update the LED to show progress
    led_state = !led_state;
    digitalWrite(RED_LED, led_state);   // turn the LED on (HIGH is the voltage level)

}
