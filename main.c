#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/spi.h"

//////////////////////////////////////////////////////////////////////////////

uint32_t adc_fifo_out = 0;
void init_adc();
void init_adc_freerun();
void init_dma();
void init_input();
void find_freq();
void get_pitch();
void get_vol();
void init_wavetable(void);
void set_freq(int chan, float f);
void init_pwm_audio();

// when testing inputs
// #define INPUT

// when testing audio
// #define AUDIO

// when testing integration
#define RUN

// segment encoding for musical notes (A-G)
extern const uint16_t msg[];
extern const float natural_piano_freqs[];
extern const uint8_t seg_note[];
extern const char* natural_piano_keys[];

// extern uint32_t adc_fifo_out;
extern int SENSOR_GPIO;
extern int ADC_CHAN;

//////////////////////////////////////////////////////////////////////////////

int main()
{
    // Configures our microcontroller to 
    // communicate over UART through the TX/RX pins
    stdio_init_all();

    // Setting the definitions

    
    #ifdef RUN
    while (true) {


    }
    #endif
    
    return 0;
}

