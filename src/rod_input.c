#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/regs/clocks.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "hardware/adc.h"
#include "hardware/dma.h"

//////////////////////////////////////////////////////////////////////////////

// gets freq from antenna through dma
// output should be usable for shriya

// Constants
const int VOL_PIN = 37;
const int ADC_PIN = 41;
const int ADC_CHAN = ADC_PIN - 40;

extern float MAX_VOL;

const float STANDARD_FREQUENCY = 4202.3869557909;

extern uint32_t adc_fifo_out = 0;

void init_adc();
void init_adc_freerun();
void init_dma();
void init_input();
void find_freq();
void get_pitch();
void get_vol();

//////////////////////////////////////////////////////////////////////////////

void init_adc() {
    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(ADC_CHAN); // verify using channel 5
}

void init_adc_freerun() {
    init_adc();
    adc_run(true); // start many
}

void init_dma() {
    dma_channel_get_default_config(0);

    dma_hw -> ch[0].read_addr = (uint32_t)&adc_hw -> fifo;
    dma_hw -> ch[0].write_addr = (uint32_t)&adc_fifo_out;

    dma_hw -> ch[0].transfer_count = (1u << DMA_CH0_TRANS_COUNT_MODE_LSB) | (1 << DMA_CH0_TRANS_COUNT_COUNT_LSB); // verify this (may not need count_count);

    uint32_t temp = 0;
    temp |= (1 << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB); // verify size of each data transfer
    temp |= (DREQ_ADC << DMA_CH0_CTRL_TRIG_TREQ_SEL_LSB);
    temp |= (1u << DMA_CH0_CTRL_TRIG_EN_LSB);
    dma_hw -> ch[0].ctrl_trig = temp;
}

void init_input() {
    // init input pin not connected to adc
    gpio_init(VOL_PIN);

    // init input through adc 
    init_dma();
    init_adc_freerun();
    adc_fifo_setup(true, true, 1, true, true);
}

void find_freq(uint src) {
    // ideal source is a rectangle wave
    fc_hw_t *fc = &clocks_hw -> fc0;
    
    // wait if fc is already running
    while(fc -> status & CLOCKS_FC0_STATUS_RUNNING_BITS) {
        tight_loop_contents();
    }

    fc -> ref_khz = clock_get_hz(clk_ref) / 1000; // change to clk_ref to clk_sys for more accuracy (6 MHz to 150 MHz)
    fc -> interval = 15; // The test interval is 0.98us * 2**interval (see table 542 for relationship to accuracy)
    fc -> min_khz = 0; // min pass freq (triggers pass/fail flag, too much work)
    fc -> max_khz = 0xffffffff; // max pass freq (triggers pass/fail flag, too much work)
    fc -> src = src; // sets source, automatically starts measurment
    
    // wait til done
    while(!(fc -> status & CLOCKS_FC0_STATUS_DONE_BITS)) {
        tight_loop_contents();
    }

    return fc -> result >> CLOCKS_FC0_RESULT_KHZ_LSB;
}

void get_vol() {
    // gets amplitude variables from volume control
    // gets amplitude value from adc
    // returns percentage of max volume (percent)
    
    // audible range from 20 Hz to 20 kHz
}

void get_pitch() {
    // use schmitt trigger to fileer out noise 
    // gets freq. from pitch control using gpio pin
    // returns 
}

