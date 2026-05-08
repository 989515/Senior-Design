//main.c
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
#include "hardware/pwm.h"

/////////////////////////////////////////////////////////////////////////////
void init_conversions();
float find_freq(float time_passed);
// void set_freq(int chan, float f);
// void set_vol(float volumePercent);
void setup_profile_buttons(void);
void setup_audio_processing(void);
void setup_audio_timer(void);

typedef enum {
    PROFILE_AUTOTUNE = 0,
    PROFILE_DISTORTION = 1,
    PROFILE_AMBIENT = 2
} SoundProfile;

extern SoundProfile current_profile;
void set_profile(SoundProfile profile);
// when testing inputs
#define TEST

// #define TEST_FREQ

// when testing integration
// #define RUN

uint32_t adc_fifo_out = 0;
volatile float freq = 440.0;
volatile float percent = 0.5;
volatile float vol = .5;
uint32_t wrap_count = 0;
uint16_t signal_count = 0;
int counter_flag = 0;
uint32_t prevTotal = 0;
absolute_time_t start_time;
extern float input_frequency;
extern int16_t sample_int16;
extern void output_sample_to_pwm(int16_t sample, float volume);
extern void update_frequency_from_input(void);
float cal_freq = 2000;

//////////////////////////////////////////////////////////////////////////////

int main() {
    
    // Configures our microcontroller to 
    // communicate over UART through the TX/RX pins
    stdio_init_all();
    sleep_ms(2000);
    init_conversions();

    set_profile(PROFILE_AUTOTUNE);
    printf("hello world\n");
    setup_audio_processing();   
    printf("Audio processing setup complete.\n");
    // setup_audio_timer();
    printf("Audio processing active!\n");
    // Default setting
    // set_freq(0, freq);
    int samples = 100;
    float time_passed = .002; // seconds


    // intialize buttons
    setup_profile_buttons();
    #ifdef TEST

    while (true) {
        //keyboard comands for key switch (testing)
       
        // test adc input + dma
        float freq_sum = 0;
        float adc_sum = 0;
        // float amp = (adc_fifo_out * 3.3) / 4095.0;
        find_freq(time_passed);
        for (int i = 0; i < samples; i++) {
            sleep_ms(time_passed * 1000);
            adc_sum += (adc_fifo_out * 3.3) / 4095.0;
            freq_sum += find_freq(time_passed); // Hz
        }

        float amp = adc_sum / samples;
        float og_freq = (freq_sum / samples);
        freq = (og_freq - cal_freq);
        if (freq < 0) freq = 0;
        // freq = freq - (((int)freq % 1000) * (1 / 8));

        // for (int i = 0; i < samples; i++) {
        //     adc_sum += (adc_fifo_out * 3.3) / 4095.0;
        //     freq_sum += find_freq(time_passed);
        //     sleep_ms(time_passed * 100);
        // }

        // float amp = adc_sum / samples;
        // freq = freq_sum / samples;
        vol = amp / 3.3;
        percent = vol;

        // if (vol <= 2.0 & vol >= 1.0) percent = .5;
        // else if (vol > 2.0) percent += .1;
        // else if (vol < 1.0) percent -= .1;

        // if (vol < .1) percent = 0.0;
        // else if (vol > 3.0) percent = 1.0;
        // else if (vol >= 0.1 && vol <= )
        // if (percent > 1.0) percent = 1.0;
        // if (percent < 0.0) percent = 0.0;
        // float clamp = fmaxf(0.0, fminf(3.3f,vol));
        // percent = clamp/3.3;
        float max_amp = 3.0;
        if (amp >= 1.0 * max_amp)  percent = 1.00f; 
        else if (amp >= .9 * max_amp)  percent = 0.90f; 
        else if (amp >= .8 * max_amp)  percent = 0.80f; 
        else if (amp >= .7 * max_amp)  percent = 0.70f; 
        else if (amp >= .6 * max_amp)  percent = 0.60f; 
        else if (amp >= .5 * max_amp)  percent = 0.50f; 
        else if (amp >= .4 * max_amp)  percent = 0.40f; 
        else if (amp >= .3 * max_amp)  percent = 0.30f; 
        else if (amp >= .2 * max_amp)  percent = 0.20f; 
        else if (amp >= .1 * max_amp)  percent = 0.10f; 
        else percent = 0.00f;
        // percent = .5;
        // set_freq(0, freq);
        // set_vol(percent);
        //freq = 440.0f; // for testing
        // freq = amp * 5000.0f; // map 0-3.3V to 0-5000Hz
        printf("ADC Result: %1.4f V \t", amp);
        printf("OG Freq.: %8.3f     ", og_freq);
        printf("Cal. Factor: %5.f     ", cal_freq);
        printf("Freq.: %8.3f     ", freq);
        printf("Volume: %2.1f %% \n", percent * 100);
        update_frequency_from_input();
        sleep_ms(10);
        // output_sample_to_pwm(sample_int16, percent);
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            switch (c) {
                case '0':
                    set_profile(PROFILE_AUTOTUNE);
                    printf("\n>>> Keyboard: Profile 0 (Auto-Tune)\n");
                    break;
                case '1':
                    set_profile(PROFILE_DISTORTION);
                    printf("\n>>> Keyboard: Profile 1 (Distortion)\n");
                    break;
                case '2':
                    set_profile(PROFILE_AMBIENT);
                    printf("\n>>> Keyboard: Profile 2 (Ambient)\n");
                    break;
                case 's':
                case 'S':
                    printf("\n========================================\n");
                    printf("  SYSTEM STATUS\n");
                    printf("========================================\n");
                    printf("Antenna frequency: %.2f Hz\n", freq);
                    printf("Volume: %.1f%%\n", percent * 100.0f);
                    printf("Current profile: %d ", current_profile);
                    switch(current_profile) {
                        case PROFILE_AUTOTUNE: printf("(Auto-Tune)\n"); break;
                        case PROFILE_DISTORTION: printf("(Distortion)\n"); break;
                        case PROFILE_AMBIENT: printf("(Ambient)\n"); break;
                    }
                    printf("ADC value: %lu\n", adc_fifo_out);
                    printf("========================================\n\n");
                    break;
                case '.':
                    cal_freq += 100;
                    break;
                case ',':
                    cal_freq -= 100;
                    break;   
                case '/': // calibrate from zero
                    cal_freq = 0;
                    break;
                case 'b': 
                    cal_freq = 1000;
                    break;
                case 'n': // when someone's playing at the instrument
                    cal_freq = 2000;
                    break;
                case 'm': // when resting on keyboard
                    cal_freq = 3000;
                    break;                 
            }
        }

        
        // printf("Frequency: %8.3f    \n", input_frequency);
        fflush(stdout);
        sleep_ms(50);
    }
    #endif



    #ifdef RUN
    while (true) {

    }
    #endif
    
    return 0;
}