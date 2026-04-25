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

//////////////////////////////////////////////////////////////////////////////

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
float freq = 440.0;
float percent = 0.5;
uint32_t wrap_count = 0;
uint16_t signal_count = 0;
int counter_flag = 0;
uint32_t prevTotal = 0;
absolute_time_t start_time;
extern float input_frequency;

//////////////////////////////////////////////////////////////////////////////

int main() {
    // Configures our microcontroller to 
    // communicate over UART through the TX/RX pins
    stdio_init_all();
    sleep_ms(2000);

    init_conversions();

    setup_audio_processing();   
    setup_audio_timer();

    // Default setting
    // set_freq(0, freq);
    int samples = 150;
    float time_passed = .0015; // seconds


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
        sleep_ms(time_passed * 1000);
        for (int i = 0; i < samples; i++) {
            adc_sum += (adc_fifo_out * 3.3) / 4095.0;
            freq_sum += find_freq(time_passed); // Hz
            sleep_ms(time_passed * 1000);
        }

        float amp = adc_sum / samples;
        freq = freq_sum / samples;
        // freq = freq - (((int)freq % 1000) * (1 / 8));

        // for (int i = 0; i < samples; i++) {
        //     adc_sum += (adc_fifo_out * 3.3) / 4095.0;
        //     freq_sum += find_freq(time_passed);
        //     sleep_ms(time_passed * 100);
        // }

        // float amp = adc_sum / samples;
        // freq = freq_sum / samples;
        percent = amp / 3.3;
        // set_freq(0, freq);
        // set_vol(percent);

        // printf("ADC Result: %1.4f    ", amp);
        printf("Freq.: %8.3f    ", freq);
        printf("Volume: %2.2f%%   ", percent * 100);

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
            }
        }

        
        printf("Frequency: %8.3f    \n", input_frequency);
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