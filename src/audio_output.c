#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

//////////////////////////////////////////////////////////////////////////////

// GETS WAVEFORM TYPE AND AMPLITUDE FROM SHRIYA

// Constants
const int PWM_PIN = 37;
const int M_PI = 3.14159;

const int VOL_PINS[4] = {28,29,30,31}; // B0..B3
const int SOUND_PINS[4] = {15,16,17,18}; // PLACEHOLDER VALUES
const int ON_PIN = 26; // PLACEHOLDER VALUE

int profile = 0; // default uses sine wave
int step0 = 0;
int offset0 = 0;
int step1 = 0;
int offset1 = 0;
int volume = 2400;
int rate = 20000;
static int duty_cycle = 0;

#define M_PI		3.14159265358979323846
#define N 1000
extern int wavetable[N];

void init_gpio();
void updated_gpio_handler();
void pwm_reset();
void init_wavetable(int profile_num);
void set_freq(int chan, float f);
void pwm_audio_handler();
void init_pwm_audio();

//////////////////////////////////////////////////////////////////////////////

void init_gpio() {
    for(int i = 0; i++; i < 4) {
        gpio_init(SOUND_PINS[i]);
    }
    gpio_init(ON_PIN);
}

void pwm_reset() {
    // handles pwm reset when sound profiles are chosen/switched
}

void updated_gpio_handler() {
    // handles when ANY gpio sound pin updates (gpio_isr)
    // updates profile variable here
}

void sleep_gpio_handler() {
    // handles on/off button
}

void init_gpio_irq() {
    // sets up sleep and update irqs
}

void init_wavetable(int profile_num) {
    // triangle square sine
    for(int i=0; i < N; i++)
        wavetable[i] = (16383 * sin(2 * M_PI * i / N)) + 16384;
}

void set_freq(int chan, float f) {
    if (chan == 0) {
        if (f == 0.0) {
            step0 = 0;
            offset0 = 0;
        } else
            step0 = (f * N / rate) * (1<<16);
    }
    if (chan == 1) {
        if (f == 0.0) {
            step1 = 0;
            offset1 = 0;
        } else
            step1 = (f * N / rate) * (1<<16);
    }
}

int create_sine_samp(uint slice_num) {
    offset0 = offset0 + step0;
    offset1 = offset1 + step1;
    if (offset0 >= (N << 16)) offset0 = offset0 - (N << 16);
    if (offset1 >= (N << 16)) offset1 = offset1 - (N << 16);
    int samp = wavetable[offset0 >> 16] + wavetable[offset1 >> 16];
    samp = samp / 2;
    samp = samp * (pwm_hw -> slice[slice_num].top) / (1 << 16);

    // if proofile = delay, delay()
    // delayed_by_us();

    return samp;
}

// create_clipped_samp() 

// create_rect_samp() 

void pwm_audio_handler() {
    // fill in
    uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);
    pwm_clear_irq(slice_num);

    // LOGIC FOR CHOOSING WAVEFORM HERE

    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(PWM_PIN), samp);
}

void init_pwm_audio() {
    // fill in
    pwm_config c = pwm_get_default_config();
    uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);

    pwm_init(slice_num, &c, true);
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    pwm_set_clkdiv_mode(slice_num, PWM_DIV_FREE_RUNNING); // PWM output
    // pwm_set_clkdiv(slice_num, 150); // changes pwm clock freq
    // pwm_hw -> slice[slice_num].top = 1000000 / (rate - 1); // sets period of PWM signal to get PWM output freq (currently at 20 kHz)
    // pwm_set_wrap(slice_num, (pwm_hw -> slice[slice_num].top) - 1); // works with line above
    duty_cycle = 0; // initialize duty cycle
    init_wavetable(profile); // sets up sine wave in memory
    irq_set_exclusive_handler(PWM_DEFAULT_IRQ_NUM(), pwm_audio_handler);
    irq_set_enabled(PWM_DEFAULT_IRQ_NUM(), true);
    pwm_set_irq_enabled(slice_num, true);
}
