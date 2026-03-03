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

void init_input();
void init_gpio();
void init_gpio_irq();
float edge_timer_start();
// void init_wavetable(void);
// void set_freq(int chan, float f);
// void init_pwm_audio();

// when testing inputs
#define TEST

// when testing integration
// #define RUN

uint32_t adc_fifo_out = 0;
float freq = 0;

//////////////////////////////////////////////////////////////////////////////

int main() {
    // Configures our microcontroller to 
    // communicate over UART through the TX/RX pins
    stdio_init_all();

    // Initializing
    init_input();

    init_gpio();
    init_gpio_irq();

    // Setting the definitions


    #ifdef TEST
    // char buffer[10];
    // printf("hellloooooo\n");
    while (true) {
        // test adc input + dma
        float f = (adc_fifo_out * 3.3) / 4095.0;
        // snprintf(buffer, sizeof(buffer), "%1.7f", f);
        // printf("ADC Result: %s     \r", buffer);
        printf("ADC Result: %f     \t", f);

        // test frequency counter
        freq = edge_timer_start();
        printf("Frequency: %8.6f   \n", freq);

        fflush(stdout);
        sleep_ms(250);

    }
    #endif



    #ifdef RUN
    while (true) {

    }
    #endif
    
    return 0;
}