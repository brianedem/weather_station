#include <Arduino.h>
#include <SPI.h>
#include <Streaming.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "sx1231.h"

unsigned radio_chip_select;
unsigned radio_reset;
unsigned radio_interrupt;

int rssi;
unsigned frequency = 0xE4C020;
int16_t fei;
int16_t afc;

struct radioInitValue {
    unsigned char regAddr;
    unsigned char regValue;
} radioInitValues[] = {
        // manufacturer recommneded default values
    {0x18, 0x88},   // LNA settings
    {0x19, 0x55},   // Channel Filter BW Control - increased from 5kHz to 10kHz
    {0x1A, 0x8B},   // Channel Filter BW Control during the AFC routine - reduced from 100kHz to 50kHz
    {0x26, 0x07},   // Mapping of pins DIO4 and DIO5, ClkOut Frequency
    {0x29, 0xE4},   // RSSI Threshold
    {0x2F, 0x01},   // Sync Word byte 1 - changed from default 0x00
    {0x30, 0x01},   // Sync Word byte 2
    {0x31, 0x01},   // Sync Word byte 3
    {0x32, 0x01},   // Sync Word byte 4
    {0x33, 0x01},   // Sync Word byte 5
    {0x34, 0x01},   // Sync Word byte 6
    {0x35, 0x01},   // Sync Word byte 7
    {0x36, 0x01},   // Sync Word byte 8
    {0x3C, 0x8F},   // Fifo threshold, Tx start condition
    {0x6F, 0x30},   // Fadin Margin Improvement

        // PCB specific configuration changes
    {0x11, 0x3F},   // Use Pa1 rather than Pa0 for transmitter

//  {0x37, 0x90},   // variable length packets, enable CRC
    {0x37, 0xD0},   // variable length packets, enable CRC, whitening

    {0x07, 0xE4},   // change frequency to 915MHz
    {0x08, 0xC0},   // change frequency to 915MHz
    {0x09, 0x20},   // change frequency to 915MHz
//  {0x07, 0xE5},   // change frequency to 916MHz
//  {0x08, 0x00},   // change frequency to 916MHz
//  {0x07, 0xE5},   // change frequency to 916MHz
//  {0x08, 0x00},   // change frequency to 916MHz
//  {0x09, 0x00},   // change frequency to 916MHz

        // AFC related configuration
//  {RegAfcCtrl, AfcLowBetaOn},    // for low-beta - shifts LO by 10% of rx bandwidth
//  {RegTestDagc, 0x10},
//  {RegTestAfc, 1},
    {RegPreambleMsb, 0},
    {RegPreambleLsb, 5},    // NXP recommendation when using AFC
    {0x19, 0x55},   // Channel Filter BW Control 
    {0x1A, 0x8A},   // Channel Filter BW Control during the AFC routine - double to 100kHz
    {RegAfcFei, AfcFei_AfcAutoclearOn},

        // list end marker
    {0x00, 0x00}
    };

void
printStatus(char *text)
{
#if 1
    char p[80];
    sprintf(p, "S:%8d, OpMode=%02x IrqFlags1=%02x IrqFlags2=%02x: %s\n",
        micros(), readRadio(RegOpMode), readRadio(RegIrqFlags1), readRadio(RegIrqFlags2), text);
    Serial.print(p);
#endif
}

void
setRadioMode(unsigned mode)
{
    writeRadio(RegOpMode, mode);
    unsigned start = micros();
    char buffer[80];
    while ((readRadio(RegIrqFlags1)&IrqFlags1_ModeReady)==0) {
        if ((micros()-start) > 1000000) {
            printStatus("setRadioMode() timeout");
            start += 1000000;
        }
    }
}

    // initialize the SPI interface to the radio
void
radioInit(unsigned pin_chip_select, unsigned pin_reset, unsigned pin_interrupt)
{
    radio_chip_select = pin_chip_select;
    radio_reset = pin_reset;
    radio_interrupt = pin_interrupt;

    SPI.begin();
    pinMode(radio_interrupt, INPUT_PULLUP);
    pinMode(radio_chip_select, OUTPUT);
    digitalWrite(radio_chip_select, HIGH);
    pinMode(radio_reset, OUTPUT);
    digitalWrite(radio_reset, HIGH);
    delay(10);      // assert for a minimum of 100us
    digitalWrite(radio_reset, LOW);
    delay(10);      // wait at least 5ms; but 10ms after POR

        // update default values as recommended by SX1231 datsheet
    struct radioInitValue *riv = radioInitValues;
    while (riv->regAddr != 0x00) {
        writeRadio(riv->regAddr, riv->regValue);
        riv++;
    }
}

void
writeRadio(byte reg_addr, byte value)
{
    digitalWrite(radio_chip_select, LOW);
    SPI.transfer(reg_addr | 0x80);
    SPI.transfer(value);
    digitalWrite(radio_chip_select, HIGH);
}

unsigned
readRadio(byte reg_addr)
{
    digitalWrite(radio_chip_select, LOW);
    SPI.transfer(reg_addr | 0x00);
    byte result = SPI.transfer(0x00);
    digitalWrite(radio_chip_select, HIGH);
    return result;
}

uint32_t
getFrequency()
{
    uint32_t frequency;
    frequency  = readRadio(RegFrfMsb) << 16;
    frequency |= readRadio(RegFrfMid) << 8;
    frequency |= readRadio(RegFrfLsb);
    return frequency;
}

void
setFrequency(uint32_t frequency)
{
    writeRadio(RegFrfMsb, frequency >> 16);
    writeRadio(RegFrfMid, frequency >> 8);
    writeRadio(RegFrfLsb, frequency);
}


void
waitModeReady()
{
    unsigned status;
    unsigned count;
    while ((readRadio(RegIrqFlags1) & IrqFlags1_ModeReady)==0) {
        delay(1);
        if (count++ > 1000)
            return;
    }
}

unsigned
sendFrame(char *buffer, unsigned bufferSize)
{
    unsigned status = readRadio(RegIrqFlags2);
    printStatus("Enter sendFrame");
    if (status & IrqFlags2_FifoNotEmpty) {
        Serial.print("Fifo not empty\n");
        while (readRadio(RegIrqFlags2) & IrqFlags2_FifoNotEmpty)
            readRadio(RegFifo);
    }
    printStatus("Putting radio in standby");
    setRadioMode(OpMode_Mode_STDBY);
    // waitModeReady();
    writeRadio(RegFifo, bufferSize);
    printStatus("Length written to fifo");
    while (bufferSize--)
        writeRadio(RegFifo, *buffer++);
    setRadioMode(OpMode_Mode_TX);
    printStatus("Radio in TX");
    // waitModeReady();
    while (((status=readRadio(RegIrqFlags2))&IrqFlags2_PacketSent) == 0)
        ;
    printStatus("Putting radio in standby");
    setRadioMode(OpMode_Mode_STDBY);
    // waitModeReady();
    printStatus("Exit sendFrame");
    return 0;
}

uint32_t
receiveFrame(uint8_t *m_buffer)
{
    char buffer[80];
    uint32_t count;
        // check for packet
    uint32_t start_time = millis();
    uint8_t low_value;
    uint8_t high_value;
        // diagnostics for checking for transmit carrier
    while (0) {
        while ((low_value = readRadio(RegRssiValue)) > 100) ;
        start_time = micros();
        while ((high_value = readRadio(RegRssiValue)) < 100) ;
        uint32_t stop_time = micros();
        uint32_t time_delta = stop_time-start_time;
        Serial.print(time_delta);
        Serial.print(" ");
        Serial.print(low_value);
        Serial.print(" ");
        Serial.println(high_value);
        delay(1000);
    }
    setRadioMode(OpMode_Mode_RX);
    if (1) {
        while (1) {
            if (readRadio(RegIrqFlags1) & IrqFlags1_SyncAddressMatch)
//          if (readRadio(RegIrqFlags1) & IrqFlags1_RxReady)
                break;
        }
//      writeRadio(RegAfcFei, AfcFei_AfcStart);
        writeRadio(RegAfcFei, AfcFei_FeiStart);
//      start_time = micros();
    }
    count = 0;
    while (1) {
        if (readRadio(RegIrqFlags2) & IrqFlags2_PayloadReady)
            break;
        if (count++ > (20*100000)) {
            if (1) {
                int16_t fei = (readRadio(RegFeiMsb)<<8) | readRadio(RegFeiLsb);
                int16_t afc = (readRadio(RegAfcMsb)<<8) | readRadio(RegAfcLsb);
                sprintf(buffer, "D:Fei = %d, Afc = %d, 0x%02x", fei, afc, readRadio(RegAfcFei));
                Serial.println(buffer);
//              writeRadio(RegAfcFei(AfcFei_AfcClear | AfcFei_AfcAutoOn );
            }
            else
                Serial.print(".");
//          writeRadio(RegAfcFei, AfcFei_AfcStart);
            count = 0;
        }
//      if (readRadio(RegIrqFlags1) & IrqFlags1_Rssi) {
//          sprintf(buffer, "D:rssi = %d\n", readRadio(RegRssiValue/2));
//          Serial.print(buffer);
//      }
//      printStatus("No RX");
//      return;
    }
    uint32_t end_time = micros();

        // fetch status
//  printStatus("received message");
    rssi = -(readRadio(RegRssiValue)/2);
    unsigned lna = readRadio(RegLna);
//  sprintf(buffer, "D:message received - RSII = %d, lna = %02x\n", rssi, lna);
//  Serial.print(buffer);
    count = 0;
    while (readRadio(RegIrqFlags2) & IrqFlags2_FifoNotEmpty) {
        *m_buffer++ = readRadio(RegFifo);
        count++;
    }
    if (0) {
        int16_t fei = (readRadio(RegFeiMsb)<<8) | readRadio(RegFeiLsb);
        int16_t afc = (readRadio(RegAfcMsb)<<8) | readRadio(RegAfcLsb);
        sprintf(buffer, "D:Fei = %d, Afc = %d, 0x%02x", fei, afc, readRadio(RegAfcFei));
        Serial.println(buffer);
    }
    if (0) {
        sprintf(buffer, "D:Packet time %dus", end_time-start_time);
        Serial.println(buffer);
    }
    if (1) {
//      printStatus("switching to FS");
//      setRadioMode(OpMode_Mode_FS);
        fei = (readRadio(RegFeiMsb)<<8) | readRadio(RegFeiLsb);
        afc = (readRadio(RegAfcMsb)<<8) | readRadio(RegAfcLsb);
        if (1) { //(fei > 20 || fei < -20) {
//          Serial.println("Updating frequency ");
//          frequency = getFrequency();
            frequency += (afc+fei)/8;
            setFrequency(frequency);
//          Serial.println(frequency, HEX);
        }
        
//      printStatus("switching to RX");
//      setRadioMode(OpMode_Mode_RX);
    }
    setRadioMode(OpMode_Mode_STDBY);

    return count;
}
