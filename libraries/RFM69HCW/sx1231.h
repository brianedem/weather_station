#ifndef sx1231_h
#define sx1231_h

// Register information
#define RegFifo             0x00
#define RegOpMode           0x01
#define    OpMode_SequencerOff  1<<7
#define    OpMode_ListenOn      1<<6
#define    OpMode_ListenAbort   1<<5
#define    OpMode_Mode_SLEEP     0<<2
#define    OpMode_Mode_STDBY     1<<2
#define    OpMode_Mode_FS        2<<2
#define    OpMode_Mode_TX        3<<2
#define    OpMode_Mode_RX        4<<2
#define RegFrfMsb           0x07
#define RegFrfMid           0x08
#define RegFrfLsb           0x09
#define RegAfcCtrl          0x0B
#define    AfcLowBetaOn                 1<<5
#define RegPaLevel          0x11
#define    PaLevel_Pa0On                1<<7
#define    PaLevel_Pa1On                1<<6
#define    PaLevel_Pa2On                1<<5
#define    PaLevel_OutputPower_Max      0x1F<<0
#define RegLna              0x18
#define     Lna_LnaZin_50               0<<7
#define     Lna_LnaZin_200              1<<7
#define     Lna_LnaGainSelect_AGC       0<<0
#define RegRxBw             0x19
#define RegAfcBw            0x1A
#define RegAfcFei           0x1E
#define    AfcFei_FeiDone               1<<6
#define    AfcFei_FeiStart              1<<5
#define    AfcFei_AfcDone               1<<4
#define    AfcFei_AfcAutoclearOn        1<<3
#define    AfcFei_AfcAutoOn             1<<2
#define    AfcFei_AfcClear              1<<1
#define    AfcFei_AfcStart              1<<0
#define RegAfcMsb           0x1F
#define RegAfcLsb           0x20
#define RegFeiMsb           0x21
#define RegFeiLsb           0x22
#define RegRssiValue        0x24
#define RegDioMapping1      0x25
#define    Dio0RxCrcOk                  0<<6
#define    Dio0RxPayloadReady           1<<6
#define    Dio0RxSyncAddress            2<<6
#define    Dio0RxRssi                   3<<6
#define    Dio1RxFifoLevel              0<<4
#define    Dio1RxFifoFull               1<<4
#define    Dio1RxFifoNotEmpty           2<<4
#define    Dio1RxFifoTimeout            3<<4
#define    Dio2RxFifoNotEmpty           0<<2
#define    Dio2RxData                   1<<2
#define    Dio2RxLowBat                 2<<2
#define    Dio2RxAutoMode               3<<2
#define    Dio3RxFifoFull               0<<0
#define    Dio3RxRssi                   1<<0
#define    Dio3RxSyncAddress            2<<0
#define    Dio3RxPllLock                3<<0
#define RegDioMapping2      0x26
#define    Dio4RxTimeout                0<<6
#define    Dio4RxRssi                   1<<6
#define    Dio4RxRxReady                2<<6
#define    Dio4RxPllLock                3<<6
#define    Dio5RxClockOut               0<<4
#define    Dio5RxData                   1<<4
#define    Dio5RxLowBattery             2<<4
#define    Dio5RxModeReady              3<<4
#define    ClkOut32Mhz                  0
#define    ClkOut16MHz                  1
#define    ClkOut8MHz                   2
#define    ClkOut4MHz                   3
#define    ClkOut2MHz                   4
#define    ClkOut1MHz                   5
#define    ClkOutRc                     6
#define    ClkOutOff                    7
#define RegIrqFlags1        0x27
#define    IrqFlags1_ModeReady          1<<7
#define    IrqFlags1_RxReady            1<<6
#define    IrqFlags1_TxReady            1<<5
#define    IrqFlags1_PllLock            1<<4
#define    IrqFlags1_Rssi               1<<3    // write 1 to clear
#define    IrqFlags1_Timeout            1<<2
#define    IrqFlags1_AutoMode           1<<1
#define    IrqFlags1_SyncAddressMatch   1<<0
#define RegIrqFlags2        0x28
#define    IrqFlags2_FifoFull           1<<7
#define    IrqFlags2_FifoNotEmpty       1<<6
#define    IrqFlags2_FifoLevel          1<<5
#define    IrqFlags2_FifoOverrun        1<<4    // write 1 to clear
#define    IrqFlags2_PacketSent         1<<3
#define    IrqFlags2_PayloadReady       1<<2
#define    IrqFlags2_CrcOk              1<<1
#define RegRssiThreshold    0x29
#define RegPreambleMsb      0x2C
#define RegPreambleLsb      0x2D
#define RegPacketConfig1    0x37
#define    PacketConfig1_PacketFormat   1<<7    // default 0 - fixed length
#define    PacketConfig1_DcFree_None        0<<5    // default 0 - none, 01-manch, 10-random
#define    PacketConfig1_DcFree_Manchester  1<<5
#define    PacketConfig1_DcFree_Scramble    2<<5
#define    PacketConfig1_DcFree_Mask        3<<5    // default 0 - none, 01-manch, 10-random
#define    PacketConfig1_CrcOn          1<<4    // default 1
#define    PacketConfig1_CrcAutoClearOff 1<<3   // default 0
#define    PacketConfig1_AddressFiltering_None              0<<1  // default 00 - none
#define    PacketConfig1_AddressFiltering_Station           1<<1
#define    PacketConfig1_AddressFiltering_Station_Broadcast 2<<1
#define    PacketConfig1_AddressFiltering_Mask              3<<1
#define RegFifoThresh       0x3C
#define    FifoThresh_TxStartCondition  1<<7
#define RegTestDagc         0x6F
#define RegTestAfc          0x71

void        writeRadio(byte reg_addr, byte value);
unsigned    readRadio(byte reg_addr);
void        radioInit(unsigned pin_chip_select, unsigned pin_reset, unsigned pin_interrupt);
void        setRadioMode(unsigned mode);
unsigned    sendFrame(char *buffer, unsigned bufferSize);
uint32_t    receiveFrame(uint8_t *);
void        waitModeReady();
#endif
