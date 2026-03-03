#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include "hardware/xosc.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"

//////////////////////////////////////////////////////////////////////////////

// Constants
const int PWM_PIN = 37;
// const float M_PI = (3.14159);

const int VOL_PINS[4] = {22,23,24,25}; // B0..B3
const int PROFILE_PINS[4] = {15,16,17,18}; // PLACEHOLDER VALUES
const int ON_PIN = 26; // GP21
const int OFF_PIN = 21;

int profile = 0; // default uses sine wave
int step0 = 0;
int offset0 = 0;
int step1 = 0;
int offset1 = 0;
int volume = 2400;
int rate = 20000; // max freq.
bool is_on = true;
// static int duty_cycle = 0;

#define M_PI 3.14159265358979323846
#define N 1000
// int wavetable[N];
extern float freq;

void init_gpio();
void updated_gpio_handler();
void pwm_reset();
void updated_gpio_handler();
void sleep_gpio_handler();
void init_gpio_irq();
// void init_wavetable();
void set_freq(int chan, float f);
void pwm_audio_handler();
void init_pwm_audio();

//////////////////////////////////////////////////////////////////////////////

void init_gpio() { // called at beginning of main
    for (int i = 0; i < 4; i++) {
        // gpio_init(PROFILE_PINS[i]);
        gpio_init(VOL_PINS[i]);
        gpio_set_dir(VOL_PINS[i], GPIO_OUT);
    }
    gpio_init(OFF_PIN);
    gpio_init(ON_PIN);
}

void sleep_gpio_handler() { // not called in main
    // handles on/off button
    uint32_t eventOn = gpio_get_irq_event_mask(ON_PIN);
    uint32_t eventOff = gpio_get_irq_event_mask(OFF_PIN);
    if (eventOff & GPIO_IRQ_EDGE_RISE) {
        gpio_acknowledge_irq(OFF_PIN, GPIO_IRQ_EDGE_RISE);
        sio_hw -> gpio_clr = (1ul << 22) | (1ul << 23) | (1ul << 24) | (1ul << 25);
        xosc_dormant();
    } else if (eventOn & GPIO_IRQ_EDGE_RISE) {
        gpio_acknowledge_irq(ON_PIN, GPIO_IRQ_EDGE_RISE);
        sio_hw -> gpio_set = (1ul << 22) | (1ul << 23) | (1ul << 24) | (1ul << 25);
        // pwm_reset();
    }
}

void init_gpio_irq() { // called at beginning of main
    // sets up sleep irq
    sio_hw -> gpio_set = (1ul << 22) | (1ul << 23) | (1ul << 24) | (1ul << 25);
    
    gpio_add_raw_irq_handler_masked((1u << OFF_PIN) | (1u << ON_PIN), sleep_gpio_handler);
    gpio_set_irq_enabled(OFF_PIN, GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(ON_PIN, GPIO_IRQ_EDGE_RISE, true);
    irq_set_enabled(IO_IRQ_BANK0, true);
    gpio_set_dormant_irq_enabled(ON_PIN, GPIO_IRQ_EDGE_RISE, true);

    // sets up update irqs
    // for (int i = 0; i < 4; i++) {
    //     gpio_add_raw_irq_handler_masked(1u << PROFILE_PINS[i], updated_gpio_handler);
    //     gpio_set_irq_enabled(PROFILE_PINS[i], GPIO_IRQ_EDGE_RISE, true);
    // }

}

void set_vol(float volumePercent) {
    // directly from 362 project, may need changing
    uint8_t mask;

    if      (volumePercent >= 0.95f) mask = 0b1111; // 95%
    else if (volumePercent >= 0.85f) mask = 0b1110; // 85%
    else if (volumePercent >= 0.65f) mask = 0b1011; // 65%
    else if (volumePercent >= 0.50f) mask = 0b1010; // 50%
    else if (volumePercent >= 0.40f) mask = 0b0111; // 40%
    else if (volumePercent >= 0.35f) mask = 0b0011; // 35%
    else if (volumePercent >= 0.25f) mask = 0b0010; // 25%
    else                       mask = 0b0000; // default ~20%

    gpio_put_masked(0b1111 << VOL_PINS[0], mask << VOL_PINS[0]);
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
//             pwm_reset();
//         }
//     }
// }

// void init_wavetable() { // not called in main
//     // triangle square sine
//     for (int i=0; i < N; i++)
//         wavetable[i] = (16383 * sin(2 * 3.14159 * i / N)) + 16384; // shifted to be positive values
// }

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

// int create_sine_samp(uint slice_num) { // not called in main
//     offset0 = offset0 + step0;
//     offset1 = offset1 + step1;
//     if (offset0 >= (N << 16)) offset0 = offset0 - (N << 16);
//     if (offset1 >= (N << 16)) offset1 = offset1 - (N << 16);

//     int samp = wavetable[offset0 >> 16] + wavetable[offset1 >> 16];
//     samp = samp / 2;
//     samp = samp * (pwm_hw -> slice[slice_num].top) / (1 << 16);
//     return samp;
// }

// int create_clipped_samp(uint slice_num) { // SHRIYA
//     // change offsets and whatnot like in create_sine_samp()
//     int samp = 0;
//     return samp;
// }

// int create_rect_samp(uint slice_num) { // SHRIYA
//     // change offsets and whatnot like in create_sine_samp()
//     int samp = 0;
//     return samp;
// }

// void pwm_audio_handler() { // not called in main
//     // CHANGE PROFILE PIN ASSIGNMENT
//     uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);
//     pwm_clear_irq(slice_num);

//     set_freq(0, freq); // 0 if only using one sine wave (no mixing)
//     // add chanA and chanB variable if we mix

//     int samp = -1;
//     int check_prof_pin = profile + PROFILE_PINS[0];
//     if (check_prof_pin == PROFILE_PINS[2]) samp = create_clipped_samp(slice_num);
//     else if (check_prof_pin == PROFILE_PINS[3]) samp = create_rect_samp(slice_num);
//     else samp = create_sine_samp(slice_num);
//     // LOGIC FOR CHOOSING WAVEFORM HERE (arbitrary assignment of profiles to pins)

//     pwm_set_chan_level(slice_num, pwm_gpio_to_channel(PWM_PIN), samp);
// }

// void init_pwm_audio() { // called at beginning of main
//     pwm_config c = pwm_get_default_config();
//     uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);

//     pwm_init(slice_num, &c, true);
//     gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
//     pwm_set_clkdiv_mode(slice_num, PWM_DIV_FREE_RUNNING); // PWM output

//     // NEED TO SET UP FOR PWM - DOES IT NEED MORE FOR 2 CHANNELS?
//     pwm_set_clkdiv(slice_num, 150); // changes pwm clock freq
//     pwm_hw -> slice[slice_num].top = 1000000 / (rate - 1); // sets period of PWM signal to get PWM output freq (currently at 20 kHz)
//     pwm_set_wrap(slice_num, (pwm_hw -> slice[slice_num].top) - 1); // works with line above
//     // duty_cycle = 0; // initialize duty cycle

//     init_wavetable(); // sets up sine wave in memory
//     irq_set_exclusive_handler(PWM_IRQ_WRAP_0, pwm_audio_handler);
//     irq_set_enabled(PWM_IRQ_WRAP_0, true);
//     pwm_clear_irq(slice_num);
//     pwm_set_irq_enabled(slice_num, true);
// }
