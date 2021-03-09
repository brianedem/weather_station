#include <Arduino.h>
#include <Streaming.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "sx1231.h"

#define FREQ_915MHZ 0xE4C000
#define FREQ_1KHZ   0x10
#define RX_START_FREQ (FREQ_915MHZ + 3*FREQ_1KHZ)

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

        // module specific configuration changes
    {RegPaLevel,
        PaLevel_Pa1On |             // module has RF switch, move from PA0 to PA1
        PaLevel_OutputPower_Max     // existing default
    },

        // packet format for this application
    {RegPacketConfig1,   // variable length packets, enable CRC, whitening
        PacketConfig1_PacketFormat |    // variable length
        PacketConfig1_DcFree_Scramble |
        PacketConfig1_CrcOn |
        PacketConfig1_AddressFiltering_None
    },

        // receiver triggers too frequently if threshold is set too low
    {RegRssiThreshold, 85*2},   // negative value, so higher values is lower signal threshold

        // AFC/FEI related configuration for frequency measurement
    {RegPreambleMsb, 0},
    {RegPreambleLsb, 5},    // NXP recommendation when using AFC

        // list end marker
    {0x00, 0x00}
};

void
radioInit() {
    struct radioInitValue *riv = radioInitValues;
    while (riv->regAddr != 0x00) {
        writeRadio(riv->regAddr, riv->regValue);
        riv++;
    }
}

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
setRadioMode(uint32_t mode)
{
    writeRadio(RegOpMode, mode);
    uint32_t start = micros();
    char buffer[80];
    //sprintf(buffer, "I:setting mode %d\n", mode);
    //Serial.print(buffer);

    while ((readRadio(RegIrqFlags1)&IrqFlags1_ModeReady)==0) {
        if ((micros()-start) > 1000000) {
            printStatus("setRadioMode() timeout");
            start += 1000000;
        }
    }
//  sprintf(buffer, "I:Mode %d ready in %dus\n", mode, micros()-start);
//  Serial.print(buffer);
}

uint32_t getFrequency()
{
    return radioSpi(SPI_READ, RegFrfMsb, 0, 3);
}

void setFrequency(uint32_t frequency)
{
    radioSpi(SPI_WRITE, RegFrfMsb, frequency, 3);
}

int16_t
getFei() {
    return radioSpi(SPI_READ, RegFeiMsb, 0, 2);
}

uint32_t
sendFrame(char *buffer, unsigned bufferSize)
{
        // wait for radio to be idle
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

    setFrequency(FREQ_915MHZ);

        // set up transmit data in radio FIFO
    writeRadio(RegFifo, bufferSize);
    printStatus("Length written to fifo");
    while (bufferSize--)
        writeRadio(RegFifo, *buffer++);

        // transmit FIFO contents
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

// The receiveListen() routine enables the receiver at the specified frequency and listens
// for the requested time. Once a signal is detected (Rssi interrupt) the radio's packet reception
// progress is monitored. If the expected sync pattern is not detected (SyncAddress interrupt)
// within the expected time window from the RSSI indication the radio's packet reception is restarted.

    // RX firmware state
uint32_t    syncTimeout;
    // RX status from last packet
int16_t     fei;
uint32_t    rssi;
int8_t      agc;
int16_t     afc;
uint32_t    rxPktLength;
uint32_t    lastPacketReceptionMs;
    // RX packet timestamps
uint32_t    rssiMs;
uint32_t    rssiUs;
uint32_t    syncAddressUs;
uint32_t    payloadReadyUs;

#define SYNC_TIMEOUT_MS (20*1000)    // sync detect should occur within 15us of RSSI indication
uint32_t
receiveListen(uint8_t *m_buffer, uint32_t rx_freq, uint32_t listenTimeMs)
{
        // buffer for printing debug messages
    char buffer[80];

    enum receiverStates {
        waitForRssi,
        waitForRxReady,
        waitForSyncAddress,
        waitForPayloadReady};
    enum receiverStates receiverState;

    uint32_t listenStartTime = micros();
    syncTimeout = 0;

        // set the frequency, start up the radio
    setFrequency(rx_freq);
    setRadioMode(OpMode_Mode_RX);

        // set up interrupt IO mapping
    writeRadio(RegDioMapping1, Dio0RxRssi);

        // monitor the radio
    receiverState = waitForRssi;
    while (1) {
        uint32_t timeFromRssi = micros() - rssiUs;
        switch (receiverState) {

            // wait for the radio's packet reception process to start to get start timestamp
        case waitForRssi:
            if (readRadioIntr()==1) {
                rssiMs = millis();
                rssiUs = micros();
                writeRadio(RegDioMapping1, Dio0RxSyncAddress);
                receiverState = waitForRxReady;
            }
            break;

            // have to wait until RxReady before requesting FeiStart
        case waitForRxReady:
            if (readRadio(RegIrqFlags1) & IrqFlags1_RxReady) {
                writeRadio(RegAfcFei, AfcFei_FeiStart);
                receiverState = waitForSyncAddress;
            }
            break;

            // if we don't get sync detection before timeout it was a false start so restart
        case waitForSyncAddress:
            if (readRadioIntr()==1) {
                syncAddressUs = micros();
                writeRadio(RegDioMapping1, Dio0RxPayloadReady);
                receiverState = waitForPayloadReady;
            }
            else if (timeFromRssi > SYNC_TIMEOUT_MS) {
                syncTimeout++;
                writeRadio(RegPacketConfig2, PacketConfig2_RestartRx);
                receiverState = waitForRssi;
            }
            break;
        
            // wait for packet reception
        case waitForPayloadReady:
            if (readRadioIntr()==1) {
                payloadReadyUs = micros();
                setRadioMode(OpMode_Mode_STDBY);
                writeRadio(RegIrqFlags1, IrqFlags1_Rssi);

                    // get the data
                for (rxPktLength=0; readRadio(RegIrqFlags2)&IrqFlags2_FifoNotEmpty; rxPktLength++)
                    *m_buffer++ = readRadio(RegFifo);

                    // Collect reception info
                rssi = -(readRadio(RegRssiValue)/2);
                agc = (readRadio(RegLna)&Lna_LnaCurrentGain_mask)>>Lna_LnaCurrentGain_shift;
                fei = getFei();
                lastPacketReceptionMs = rssiMs;

                    // Print out diagnostic info
                if (0) {
                    sprintf(buffer, "I:reception complete after %dms", (micros()-listenStartTime)/1000);
                    Serial.println(buffer);
                    sprintf(buffer, "I:%dms: RSSI", rssiMs-lastPacketReceptionMs);
                    Serial.println(buffer);
                    sprintf(buffer, "I:%dus: SyncAddress", syncAddressUs-rssiUs);
                    Serial.println(buffer);
                    sprintf(buffer, "I:%dus: PayloadReady", payloadReadyUs-syncAddressUs);
                    Serial.println(buffer);
                    sprintf(buffer, "I:frequency = %x, FEI=%d, RSSI=%d, syncTimeouts=%d",
                            getFrequency(), fei, rssi, syncTimeout);
                    Serial.println(buffer);
                }

                return 0;
            }
            break;
        }
        
            // check for a timeout
        if ((micros() - listenStartTime) > (listenTimeMs*1000)) {
            setRadioMode(OpMode_Mode_STDBY);
            writeRadio(RegIrqFlags1, IrqFlags1_Rssi);
            if (receiverState != waitForRssi)
                return 3;   // reception aborted
            else if (syncTimeout !=0)
                return 2;   // false RSSI triggers
            else
                return 1;   // timeout
        }
    }
}

// receiveFrame() waits for the next frame to be received and returns it.
// When reception first starts, or after it has been interupted for an extended period,
// the exact frequency of the transmission, relative to the receiver, is unknown. If packets
// are not being reliably received at the expected interval the routine will shift the
// receiver's frequency through a sequence of frequency offsets until one that works is found.
// Once reliable reception has been established the measured frequency offset is
// used to adjust the frequency of the receiver so that it will track the transmitter,
// which varies due to transmitter's crystal temperature.
uint32_t    rxFrequency;
uint32_t    rxFreqSeekIndex = 0;
int32_t     rxFreqSeekSequence[] = {
     0*FREQ_1KHZ,
     2*FREQ_1KHZ,
    -2*FREQ_1KHZ,
     4*FREQ_1KHZ,
    -4*FREQ_1KHZ,
     6*FREQ_1KHZ,
    -6*FREQ_1KHZ,
     8*FREQ_1KHZ,
    -8*FREQ_1KHZ,
     0
};
#define PACKET_START_OFFSET 100    // amount of time to start receiver before expected packet arrival
#define PACKET_INTERVAL     5000    // packets arrive every 5 seconds
#define PACKET_RX_WINDOW    (PACKET_START_OFFSET + 150)  // listen for 50ms past expected start
#define CONNECTION_LOST_THRESHOLD   4
uint32_t    rxConnectedState = 0;
uint32_t    consectivePacketLossCount;
uint32_t    rxTimeout;
int32_t     rxFrequencyTrim;
#define AFC_ONE 250
uint32_t
receiveFrame(uint8_t *m_buffer)
{
        // buffer for printing debug messages
    char buffer[80];

    consectivePacketLossCount = 0;

        // loop until a packet is successfully received
    while (1) {

            // initialize channel info if we are starting up
        uint32_t nextPacketStart;
        if (!rxConnectedState) {
            rxTimeout = PACKET_INTERVAL + 100;      // 100ms longer than the expected packet interval
            rxFrequency = RX_START_FREQ + rxFreqSeekSequence[rxFreqSeekIndex++];
            if (rxFreqSeekSequence[rxFreqSeekIndex]==0x00)
                rxFreqSeekIndex = 0;
        }
            // otherwise delay until just before the start of the next packet
        else {
            rxTimeout = PACKET_RX_WINDOW;
            uint32_t currentTimeMs = millis();
            uint32_t missedPacketStarts = (currentTimeMs - lastPacketReceptionMs)/PACKET_INTERVAL;
            nextPacketStart = lastPacketReceptionMs + (missedPacketStarts+1)*PACKET_INTERVAL;
            int32_t delayInterval = nextPacketStart - currentTimeMs - PACKET_START_OFFSET;
            if (delayInterval < 0)
                delayInterval += PACKET_INTERVAL;
//          sprintf(buffer, "I:delay %d", delayInterval);
//          Serial.println(buffer);
            delay(delayInterval);
        }
            // attempt to receive a packet
//      sprintf(buffer, "I:receiving at %x with timeout of %d", rxFrequency, rxTimeout);
//      Serial.println(buffer);
        uint32_t rxError = receiveListen(m_buffer, rxFrequency, rxTimeout);

            // if a packet was received record the time to establish the interval

        if (rxError) {
//          sprintf(buffer, "I:packet RX timeout %d", rxError);
//          Serial.println(buffer);
            consectivePacketLossCount++;
            if (consectivePacketLossCount>=CONNECTION_LOST_THRESHOLD) {
                rxConnectedState = 0;
                Serial.println("I:RX connection lost");
            }
        }
        else
            break;
    }

        // update packet reception state, adjust the receive frequency
    if (!rxConnectedState) {
        rxConnectedState = 1;
        rxFrequencyTrim = fei;
    }
    else {
        rxFrequencyTrim += fei;
        if (rxFrequencyTrim > AFC_ONE) {
            rxFrequencyTrim -= AFC_ONE;
            rxFrequency++;
        }
        else if (rxFrequencyTrim < -AFC_ONE) {
            rxFrequencyTrim += AFC_ONE;
            rxFrequency--;
        }
    }
        // frequency tracking diagnostic info
//  sprintf(buffer, "I:rxFrequencyTrim = %d, rxFrequency = %x", rxFrequencyTrim, rxFrequency);
//  Serial.println(buffer);

    return 0;
}
