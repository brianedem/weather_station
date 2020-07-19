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
#define RegLna              0x18
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
