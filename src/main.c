#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico.h"
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/spi.h"
#include "hardware/clocks.h"

//////////////////////////////////////////////////////////////////////////////

void init_conversions();
float find_freq();
void set_freq(int chan, float f);
void set_vol(float volumePercent);

// when testing inputs
#define TEST

// #define TEST_FREQ

// when testing integration
// #define RUN

uint32_t adc_fifo_out = 0;
float freq = 440.0;
float vol_percent = 0.5;

//////////////////////////////////////////////////////////////////////////////

int main() {
    // Configures our microcontroller to 
    // communicate over UART through the TX/RX pins
    stdio_init_all();

    // Initializing
    // init_pwm_audio();

    init_conversions();

    // Default setting
    set_freq(0, freq);
    set_vol(vol_percent);


    #ifdef TEST
    // char buffer[10];
    // printf("hellloooooo\n");
    // init_counter_pwm();

    while (true) {
        // test adc input + dma
        float freq_sum = 0;
        float adc_sum = 0;
        // float amp = (adc_fifo_out * 3.3) / 4095.0;

        for (int i = 0; i < 100; i++) {
            adc_sum += (adc_fifo_out * 3.3) / 4095.0;
            freq_sum += find_freq();
            sleep_ms(2);
        }

        float amp = adc_sum / 100;
        freq = freq_sum / 100;
        vol_percent = amp / 3.3;
        set_freq(0, freq);
        set_vol(vol_percent);

        printf("ADC Result: %1.4f    ", amp);
        printf("Frequency: %8.3f    ", freq);
        printf("Volume: %1.2f    \r", vol_percent);


        fflush(stdout);
        sleep_ms(5);
    }
    #endif



    #ifdef RUN
    while (true) {

    }
    #endif
    
    return 0;
}