#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/regs/clocks.h"
#include "hardware/clocks.h"
#include "hardware/xosc.h"
#include "hardware/timer.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/spi.h"
#include "hardware/pwm.h"

//////////////////////////////////////////////////////////////////////////////

// gets freq from antenna through dma
// output should be usable for shriya

// Constants
// const int VOL_PIN = 37;
// const int PWM_PIN = 36;
const int ADC_PIN = 41;
const int FREQ_PIN = 11; // pwm b channel
const int ADC_CHAN = ADC_PIN - 40;
// extern float MAX_VOL;
// const float STANDARD_FREQUENCY = 4202.3869557909;
// const int VOL_PINS[4] = {22,23,24,25}; // B0..B3
// const int PROFILE_PINS[4] = {15,16,17,18}; // PLACEHOLDER VALUES
const int ON_PIN = 26; // GP21
const int OFF_PIN = 21;

extern uint32_t adc_fifo_out; // amplitude stored here

int profile = 0; // default uses sine wave
int step0 = 0;
int offset0 = 0;
int step1 = 0;
int offset1 = 0;
int volume = 2400;
int rate = 20000; // max freq.
// bool is_on = true;
// static int duty_cycle = 0; // CHANGE TO CHANGE VOLUME
// bool led_on = false;

#define M_PI 3.14159265358979323846
#define N 1000
int wavetable[N];
extern float freq;

int timer_dma_chan;
uint counter_slice;
// extern absolute_time_t start_time;
float frequency = 440.0;
u_int32_t pulse_count;

// extern uint32_t wrap_count;
// extern uint16_t signal_count;
// extern int counter_flag;
// extern uint32_t prevTotal;

void init_gpio();
// void pwm_reset();
void updated_gpio_handler();
void sleep_gpio_handler();
void count_pulse();
void init_gpio_irq();
void init_adc();
void init_adc_freerun();
void init_dma();
// void init_wavetable();
// int create_sine_samp(uint slice_num);
// void pwm_audio_handler();
// void init_pwm_audio();
void init_conversions();

float find_freq(float time_passed);
// void set_freq(int chan, float f);
// void set_vol(float volumePercent);

//////////////////////////////////////////////////////////////////////////////

void init_gpio() { // called in big init
    // for (int i = 0; i < 4; i++) {
    //     // gpio_init(PROFILE_PINS[i]);
    //     gpio_init(VOL_PINS[i]);
    //     gpio_set_dir(VOL_PINS[i], GPIO_OUT);
    // }
    gpio_init(OFF_PIN);
    gpio_init(ON_PIN);
    gpio_init(FREQ_PIN);
}

// void pwm_reset() { // not called in main
//     // handles pwm reset when sound profiles are chosen/switched
//     int slice_num = pwm_gpio_to_slice_num(PWM_PIN);
//     pwm_hw->slice[slice_num].ctr = PWM_CH0_CTR_RESET;
//     pwm_hw->slice[slice_num].cc = PWM_CH0_CC_RESET;
// }

// void updated_gpio_handler() { // not called in main
//     // handles when ANY gpio sound pin updates (gpio_isr)
//     // updates profile variable here
//     for (int i = 0; i < 4; i++) {
//         if (gpio_get_irq_event_mask(i + PROFILE_PINS[0]) & GPIO_IRQ_EDGE_RISE) {
//             gpio_acknowledge_irq(i + PROFILE_PINS[0], GPIO_IRQ_EDGE_RISE);
//             profile = i;
//             // pwm_reset();
//         }
//     }
// }

void sleep_gpio_handler() { // not called in main
    // handles on/off button
    uint32_t eventOn = gpio_get_irq_event_mask(ON_PIN);
    uint32_t eventOff = gpio_get_irq_event_mask(OFF_PIN);
    if (eventOff & GPIO_IRQ_EDGE_RISE) {
        gpio_acknowledge_irq(OFF_PIN, GPIO_IRQ_EDGE_RISE);
        sio_hw -> gpio_clr = (1ul << 25);
        
        xosc_dormant();
    } else if (eventOn & GPIO_IRQ_EDGE_RISE) {
        gpio_acknowledge_irq(ON_PIN, GPIO_IRQ_EDGE_RISE);
        sio_hw -> gpio_set = (1ul << 25);
        pulse_count = 0;
        // start_time = get_absolute_time();
        // pwm_reset();
    }
}

void count_pulse() {
    gpio_acknowledge_irq(FREQ_PIN, GPIO_IRQ_EDGE_RISE);
    pulse_count += 1;
}

void init_gpio_irq() { // called in bit init
    // sets up sleep irq
    sio_hw -> gpio_set = (1ul << 25);
    gpio_add_raw_irq_handler_masked((1u << OFF_PIN) | (1u << ON_PIN), sleep_gpio_handler);
    gpio_set_irq_enabled(OFF_PIN, GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(ON_PIN, GPIO_IRQ_EDGE_RISE, true);

    // sets up frequency input irq
    gpio_add_raw_irq_handler(FREQ_PIN, count_pulse);
    gpio_set_irq_enabled(FREQ_PIN, GPIO_IRQ_EDGE_RISE, true);

    irq_set_enabled(IO_IRQ_BANK0, true);
    gpio_set_dormant_irq_enabled(ON_PIN, GPIO_IRQ_EDGE_RISE, true);
    pulse_count = 0;
    // start_time = get_absolute_time();
}

void init_adc() { // called in init adc freerun
    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(ADC_CHAN); // verify using channel 5
}

void init_adc_freerun() { // called in input
    init_adc();
    adc_run(true); // start many
}

void init_dma() { // called in input
    dma_channel_get_default_config(0);
    dma_hw -> ch[0].read_addr = (uint32_t)&adc_hw -> fifo;
    dma_hw -> ch[0].write_addr = (uint32_t)&adc_fifo_out;
    dma_hw -> ch[0].transfer_count = (1u << DMA_CH0_TRANS_COUNT_MODE_LSB) | (1 << DMA_CH0_TRANS_COUNT_COUNT_LSB); // verify this (may not need count_count);

    uint32_t temp0 = 0;
    temp0 |= (1 << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB);
    temp0 |= (DREQ_ADC << DMA_CH0_CTRL_TRIG_TREQ_SEL_LSB);
    temp0 |= (1u << DMA_CH0_CTRL_TRIG_EN_LSB);

    dma_hw -> ch[0].ctrl_trig = temp0;
}

// void init_wavetable() { // called in pwm init
//     // sine
//     for (int i=0; i < N; i++)
//         wavetable[i] = (16383 * sin(2 * 3.14159 * i / N)) + 16384; // shifted to be positive values
// }

// int create_sine_samp(uint slice_num) { // called by handler
//     offset0 = offset0 + step0;
//     offset1 = offset1 + step1;
//     if (offset0 >= (N << 16)) offset0 = offset0 - (N << 16);
//     if (offset1 >= (N << 16)) offset1 = offset1 - (N << 16);

//     int samp = wavetable[offset0 >> 16] + wavetable[offset1 >> 16];
//     samp = samp / 2;
//     samp = samp * (pwm_hw -> slice[slice_num].top) / (1 << 16);
//     return samp;
// }

void init_conversions() { // put at beginning of main
    // init for gpio pins and irqs
    init_gpio();
    init_gpio_irq();

    // init input through adc 
    // init_pwm_audio();
    init_dma();
    init_adc_freerun();
    adc_fifo_setup(true, true, 1, false, false);
}

float find_freq(float time_passed) { // called in main loop
    u_int32_t last_count = pulse_count;
    pulse_count = 0;
    return (last_count / time_passed); // counts per sec = Hz
}

// void set_freq(int chan, float f) { // called in main
//     if (chan == 0) {
//         if (f == 0.0) {
//             step0 = 0;
//             offset0 = 0;
//         } else
//             step0 = (f * N / rate) * (1<<16);
//     }
//     if (chan == 1) { // this would let us mix two sine waves together (if we're using sine)
//         if (f == 0.0) {
//             step1 = 0;
//             offset1 = 0;
//         } else
//             step1 = (f * N / rate) * (1<<16);
//     }
// }
