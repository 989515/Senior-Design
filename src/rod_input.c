#include <stdio.h>
#include <string.h>
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
const int ADC_PIN = 41;
const int FREQ_PIN = 21; // pwm b channel
const int ADC_CHAN = ADC_PIN - 40;
// extern float MAX_VOL;
// const float STANDARD_FREQUENCY = 4202.3869557909;
extern uint32_t adc_fifo_out; // amplitude stored here
#define NUM_EDGE_TIMES 2 // number of cycles to wait to capture - lowest freq to get is 100Hz
#define EDGE_WAIT_MSEC 20 // time to wait to recieve edges

uint edge_times[NUM_EDGE_TIMES];
int timer_dma_chan;
uint counter_slice;

void init_adc();
void init_adc_freerun();
void init_dma();
void init_input();
void init_counter_pwm();
int edge_timer_val();
float edge_timer_start();
float find_freq();

//////////////////////////////////////////////////////////////////////////////

void init_adc() { // not called in main
    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(ADC_CHAN); // verify using channel 5
}

void init_adc_freerun() { // not called in main
    init_adc();
    adc_run(true); // start many
}

void init_dma() { // not called in main
    dma_channel_get_default_config(0);
    dma_hw -> ch[0].read_addr = (uint32_t)&adc_hw -> fifo;
    dma_hw -> ch[0].write_addr = (uint32_t)&adc_fifo_out;
    dma_hw -> ch[0].transfer_count = (1u << DMA_CH0_TRANS_COUNT_MODE_LSB) | (1 << DMA_CH0_TRANS_COUNT_COUNT_LSB); // verify this (may not need count_count);

    uint32_t temp0 = 0;
    temp0 |= (1 << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB);
    temp0 |= (DREQ_ADC << DMA_CH0_CTRL_TRIG_TREQ_SEL_LSB);
    temp0 |= (1u << DMA_CH0_CTRL_TRIG_EN_LSB);
    dma_hw -> ch[0].ctrl_trig = temp0;


    dma_channel_get_default_config(1);
    dma_hw -> ch[1].read_addr = (uint32_t)&timer_hw -> timerawl;
    dma_hw -> ch[1].write_addr = (uint32_t)edge_times;
    dma_hw -> ch[1].transfer_count = (1u << DMA_CH1_TRANS_COUNT_MODE_LSB) | (1 << DMA_CH1_TRANS_COUNT_COUNT_LSB);

    uint32_t temp1 = 0;
    temp1 |= (DMA_SIZE_32 << DMA_CH1_CTRL_TRIG_DATA_SIZE_LSB);
    temp1 |= (pwm_get_dreq(counter_slice) << DMA_CH1_CTRL_TRIG_TREQ_SEL_LSB);
    temp1 |= (1u << DMA_CH1_CTRL_TRIG_EN_LSB);
    dma_hw -> ch[1].ctrl_trig = temp1;
}

void init_counter_pwm() {
    assert(pwm_gpio_to_channel(FREQ_PIN) == PWM_CHAN_B);
    counter_slice = pwm_gpio_to_slice_num(FREQ_PIN);
 
    gpio_set_function(FREQ_PIN, GPIO_FUNC_PWM);
    pwm_config pc = pwm_get_default_config();
    pwm_config_set_clkdiv_mode(&pc, PWM_DIV_B_RISING);
    pwm_config_set_clkdiv(&pc, 1);
    pwm_init(counter_slice, &pc, false);

    // timer_dma_chan = dma_claim_unused_channel(true);
    // dma_channel_config dc = dma_channel_get_default_config(timer_dma_chan);

    // channel_config_set_transfer_data_size(&dc, DMA_SIZE_32);
    // channel_config_set_read_increment(&dc, false);
    // channel_config_set_write_increment(&dc, true);
    // channel_config_set_dreq(&dc, pwm_get_dreq(counter_slice));
    // dma_channel_configure(timer_dma_chan, &dc, edge_times, &timer_hw->timerawl, NUM_EDGE_TIMES, false);
    pwm_set_wrap(counter_slice, 0);
}

float edge_timer_start() {
    // memset(edge_times, 0, sizeof(edge_times));
    pwm_set_counter(counter_slice, 0);
    pwm_set_enabled(counter_slice, true);
    sleep_ms(EDGE_WAIT_MSEC);
    return find_freq();
}

int edge_timer_val() {
    uint i=1, n;
    int total=0;
 
    dma_channel_abort(1);
    pwm_set_enabled(counter_slice, false);    
    while (i<NUM_EDGE_TIMES && edge_times[i]) {
        n = edge_times[i] - edge_times[i-1];
        total += n;
        i++;
    }
    dma_channel_start(1);
    return(i>1 ? total / (i - 1) : 0);
}

float find_freq() {
    // ideal source is a rectangle wave
    // maxes out at ~1.05kHz freq.
    // use odd pwm pins (b channels)
    int val = edge_timer_val();
    return (val ? 1e6 / val : 0);
}

void init_input() { // put at beginning of main
    // init for frequency counter stuff
    init_counter_pwm();

    // init input through adc 
    init_dma();
    init_adc_freerun();
    adc_fifo_setup(true, true, 1, false, false);
}
