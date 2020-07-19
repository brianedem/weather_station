#include <Arduino.h>
#include "ardent.h"

    // Timer constants
#define CPU_HZ 48000000
#define TIMER_PRESCALER_DIV 64
#define DEBOUNCE_INTERVAL   3
#define INTERVAL_3MS (CPU_HZ * DEBOUNCE_INTERVAL / 1000 / TIMER_PRESCALER_DIV)

extern unsigned ardent_wind_count;
extern unsigned ardent_direction;      // angle
extern unsigned ardent_rainfall;

// The reed switches in the Ardent weather station bounce when the contacts close
// To mask the bounce the switch closure handlers will start a timer on the first
// contact and ignore any additional switch closure indications for ~3ms
TcCount16* rainTC = (TcCount16*)TC3;
void
rain_handler()
{
    if (rainTC->STATUS.bit.STOP) {
        ardent_rainfall++;

            // restart the timer
        rainTC->COUNT.reg = INTERVAL_3MS;
        while (rainTC->STATUS.bit.SYNCBUSY == 1) ;
        rainTC->CTRLBSET.reg = TC_CTRLBSET_CMD_RETRIGGER;
        while (rainTC->STATUS.bit.SYNCBUSY == 1) ;
    }
}
TcCount16* windTC = (TcCount16*)TC4;
void
wind_handler()
{
    if (windTC->STATUS.bit.STOP) {
        ardent_wind_count++;

            // restart the timer
        windTC->COUNT.reg = INTERVAL_3MS;
        while (windTC->STATUS.bit.SYNCBUSY == 1) ;
        windTC->CTRLBSET.reg = TC_CTRLBSET_CMD_RETRIGGER;
        while (windTC->STATUS.bit.SYNCBUSY == 1) ;
    }
}

// Arduino does not directly support timers, so here is some code to set up a one-shot timer
// The timer counts down from the initial value in COUNT, and when it reaches zero it stops
// with STOP status in the STATUS register.
void
timer_init(TcCount16* TC)
{

        // disable timer before programming (should already be off)
    TC->CTRLA.reg &= ~TC_CTRLA_ENABLE;
    while (TC->STATUS.bit.SYNCBUSY == 1) ;

    TC->CTRLA.reg |=
            // use a 16-bit timer
        TC_CTRLA_MODE_COUNT16 |
            // use match mode so that the timer counter resets/stops when count matches the compare register
        TC_CTRLA_WAVEGEN_MFRQ |
            // set prescaler to 64
        TC_CTRLA_PRESCALER_DIV64;
    while (TC->STATUS.bit.SYNCBUSY == 1) ;

        // set one-shot mode and have counter count down
    TC->CTRLBSET.reg = TC_CTRLBSET_ONESHOT | TC_CTRLBSET_DIR;
    while (TC->STATUS.bit.SYNCBUSY == 1) ;

        // set a low starting value so it immediatly stops
    TC->COUNT.reg = 1;
    while (TC->STATUS.bit.SYNCBUSY == 1) ;

        // enable timer
    TC->CTRLA.reg |= TC_CTRLA_ENABLE;
    while (TC->STATUS.bit.SYNCBUSY == 1) ;
}
void
ardentInit()
{
    // set up the event handlers
    attachInterrupt(digitalPinToInterrupt(ARDENT_RAIN), rain_handler, FALLING);
    attachInterrupt(digitalPinToInterrupt(ARDENT_SPEED), wind_handler, FALLING);
    
    // configure the ADC
    analogReadResolution(12);

    // Timer Counter
        // enable the clock to TC3
    REG_GCLK_CLKCTRL = (uint16_t) (GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_TCC2_TC3);
    while (GCLK->STATUS.bit.SYNCBUSY == 1) ;    // wait for sync to prevent missing interrupts

    timer_init(rainTC);

        // enable the clock to TC4
    REG_GCLK_CLKCTRL = (uint16_t) (GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_TC4_TC5);
    while (GCLK->STATUS.bit.SYNCBUSY == 1) ;    // wait for sync to prevent missing interrupts

    timer_init(windTC);
}

unsigned
ardent_dir(void)
{
    ardent_direction = analogRead(ARDENT_DIR);
}
// Wind information is collected every 5 seconds for two minutes and then the results are
// processed to determine the average speed and direction, the speed of any gusts (if any), and 

/*
uint32_t wind_speed_history[120];   // two minutes of wind speed history collected every 5 seconds
uint32_t wind_dir_history[120];     // two minutes of wind direction history collected every 5 seconds
uint32_t previous_wind_count = 0;   // windspeed count at previous call to ardentUpdate
unsigned index_1m = 0;
unsigned index_2m = 0;
unsigned index_1h = 0;


// This routine needs to be called every 5 seconds
void
ardentUpdate()
{

    // Wind speed is an average speed for the most recent two-minute period prior to the observation
    // A wind gust is the peak instananeous wind speed in the most recent 10 minutes that is 10 knots greater than the lowest lull
    // If the maximum instananeous wind since the previous report exceeds 25 knots, that is reported as peak wind
    // Instananeous speed is measured over a 5 second period
    // one knot is 0.515m/s
    // Wind direction is the angle from 1 to 360 degrees. For calm winds (less than 1 knott) the reported angle is 0
    uint32_t current_wind_count = ardent_wind_count;
    uint32_t wind_speed = current_wind_count - previous_wind_count;  // this works even if the count wraps
    wind_speed_history[index_2m] = wind_speed;
    ardent_direction = 0;
    int i;
    for (i=0; i<16; i++)
        ardent_direction += analogRead(ARDENT_DIR);
    ardent_direction >>= 4;
    Adc* adc = (Adc*)ADC;
}
*/
