/*
 * sound_profiles.c
 * Digital Theremin - Sound Profile Processing with PWM Output
 * 
 * Implements 3 sound profiles:
 * - Profile 0: Auto-Tune (Sine wave with pitch correction)
 * - Profile 1: Distortion (Sawtooth wave with soft clipping)
 * - Profile 2: Ambient Pad (Smooth, spacious, dreamy)
 * 
 * Sample Rate: 44,100 Hz
 * Output: PWM on GPIO 14
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"


// ============================================================
// CONFIGURATION & CONSTANTS
// ============================================================

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE 44100
#define WAVETABLE_SIZE 256
#define NUM_NOTES 60
#define A4_FREQUENCY 440.0f
#define PHASE_MAX 4294967296.0f

#define AUTOTUNE_GLIDE_RATE 0.8f
#define AUTOTUNE_STRENGTH 1.0f
#define FREQ_CHANGE_THRESHOLD 1.0f

#define DISTORTION_AMOUNT 0.85f
#define DISTORTION_DRIVE_MAX 20.0f

#define SAMPLES_PER_CALLBACK 100
#define CALLBACK_PERIOD_US ((SAMPLES_PER_CALLBACK * 1000000) / SAMPLE_RATE)

#define PWM_OUTPUT_PIN 14
#define DEBUG_LED_PIN 25
#define PWM_WRAP_VALUE 65535

#define BUTTON_PROFILE_0  15  // Auto-Tune button
#define BUTTON_PROFILE_1  16  // Distortion button
#define BUTTON_PROFILE_2  17  // Ambient button

// Debounce timing (milliseconds)
#define BUTTON_DEBOUNCE_MS 200

// ============================================================
// ENUMERATIONS
// ============================================================

// Pre-defined piano key frequencies for auto-tune
const float piano_keys_frequencies[38] = {
    110.0000, 123.4708, 130.8128, 146.8324,
    164.8138, 174.6141, 195.9977, 220.0000, 246.9417, 261.6256,
    293.6648, 329.6276, 349.2282, 391.9954, 440.0000, 493.8833,
    523.2511, 587.3295, 659.2551, 698.4565, 783.9909, 880.0000,
    987.7666, 1046.502, 1174.659, 1318.510, 1396.913, 1567.982,
    1760.000, 1975.533, 2093.005, 2349.318, 2637.020, 2793.826,
    3135.963, 3520.000, 3951.066, 4186.009
};

#define NUM_PIANO_KEYS 38
#define MIN_AUTOTUNE_FREQ 110.0f   // Lowest note
#define MAX_AUTOTUNE_FREQ 4186.009f // Highest note

typedef enum {
    WAVEFORM_SINE = 0,
    WAVEFORM_SQUARE = 1,
    WAVEFORM_SAWTOOTH = 2,
    WAVEFORM_TRIANGLE = 3
} WaveformType;

typedef enum {
    PROFILE_AUTOTUNE = 0,
    PROFILE_DISTORTION = 1,
    PROFILE_AMBIENT = 2,
    NUM_PROFILES = 3
} SoundProfile;

// ============================================================
// GLOBAL DATA
// ============================================================

volatile uint32_t last_button_time = 0;

// Wavetables
float sine_table[WAVETABLE_SIZE];
float square_table[WAVETABLE_SIZE];
float sawtooth_table[WAVETABLE_SIZE];
float triangle_table[WAVETABLE_SIZE];

static uint16_t pwm_wrap_value_stored = 0;

// Oscillator state
uint32_t phase_main = 0;
uint32_t phase_detune1 = 0;
uint32_t phase_detune2 = 0;
uint32_t phase_octave = 0;
uint32_t phase_increment_main = 0;
uint32_t phase_increment_detune1 = 0;
uint32_t phase_increment_detune2 = 0;
uint32_t phase_increment_octave = 0;
WaveformType current_waveform = WAVEFORM_SINE;

// Auto-tune state
float current_freq = 440.0f;
float target_freq = 440.0f;
float last_input_freq = 440.0f;
extern float percent;

extern float find_freq(float time_passed);
extern float freq;
// Profile
SoundProfile current_profile = PROFILE_AUTOTUNE;

// PWM
uint pwm_slice_num;

// Effects
float tremolo_phase = 0.0f;
float vibrato_phase = 0.0f;

// Low-pass filter
#define FILTER_SIZE 5
float filter_buffer[FILTER_SIZE] = {0};
int filter_index = 0;

// Counters
volatile uint32_t total_samples_generated = 0;
volatile uint32_t sample_counter = 0;
volatile bool debug_print_flag = false;
volatile int16_t last_sample_output = 0;

float input_frequency;

// ============================================================
// BUTTON INTERRUPT HANDLER
// ============================================================

void button_irq_handler(uint gpio, uint32_t events) {
    // Debounce - ignore button presses within 200ms of last press
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_button_time < BUTTON_DEBOUNCE_MS) {
        return;
    }
    last_button_time = now;
    
    // Determine which button was pressed
    if (gpio == BUTTON_PROFILE_0) {
        set_profile(PROFILE_AUTOTUNE);
        printf("\n>>> Button Press: Profile 0 (Auto-Tune)\n");
    }
    else if (gpio == BUTTON_PROFILE_1) {
        set_profile(PROFILE_DISTORTION);
        printf("\n>>> Button Press: Profile 1 (Distortion)\n");
    }
    else if (gpio == BUTTON_PROFILE_2) {
        set_profile(PROFILE_AMBIENT);
        printf("\n>>> Button Press: Profile 2 (Ambient)\n");
    }
}

// ============================================================
// BUTTON SETUP
// ============================================================

void setup_profile_buttons(void) {
    // Initialize button GPIOs
    gpio_init(BUTTON_PROFILE_0);
    gpio_init(BUTTON_PROFILE_1);
    gpio_init(BUTTON_PROFILE_2);
    
    // Set as inputs
    gpio_set_dir(BUTTON_PROFILE_0, GPIO_IN);
    gpio_set_dir(BUTTON_PROFILE_1, GPIO_IN);
    gpio_set_dir(BUTTON_PROFILE_2, GPIO_IN);
    
    // Enable internal pull-up resistors
    // (buttons connect to GND when pressed)
    gpio_pull_up(BUTTON_PROFILE_0);
    gpio_pull_up(BUTTON_PROFILE_1);
    gpio_pull_up(BUTTON_PROFILE_2);
    
    // Set up interrupts on falling edge (button press = high→low)
    gpio_set_irq_enabled_with_callback(
        BUTTON_PROFILE_0, 
        GPIO_IRQ_EDGE_FALL, 
        true, 
        &button_irq_handler
    );
    
    gpio_set_irq_enabled(BUTTON_PROFILE_1, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BUTTON_PROFILE_2, GPIO_IRQ_EDGE_FALL, true);
    
    printf("Profile buttons initialized:\n");
    printf("  GPIO %d - Auto-Tune\n", BUTTON_PROFILE_0);
    printf("  GPIO %d - Distortion\n", BUTTON_PROFILE_1);
    printf("  GPIO %d - Ambient\n", BUTTON_PROFILE_2);
}
// ============================================================
// WAVETABLE INITIALIZATION
// ============================================================

void init_wavetables(void) {
    // Sine
    for (int i = 0; i < WAVETABLE_SIZE; i++) {
        float phase_pos = (float)i / (float)WAVETABLE_SIZE;
        sine_table[i] = sinf(2.0f * M_PI * phase_pos);
    }
    
    // Square
    for (int i = 0; i < WAVETABLE_SIZE; i++) {
        square_table[i] = (i < WAVETABLE_SIZE / 2) ? 1.0f : -1.0f;
    }
    
    // Sawtooth
    for (int i = 0; i < WAVETABLE_SIZE; i++) {
        float phase_pos = (float)i / (float)WAVETABLE_SIZE;
        sawtooth_table[i] = 2.0f * phase_pos - 1.0f;
    }
    
    // Triangle
    for (int i = 0; i < WAVETABLE_SIZE; i++) {
        float phase_pos = (float)i / (float)WAVETABLE_SIZE;
        if (phase_pos < 0.25f) {
            triangle_table[i] = 4.0f * phase_pos;
        } else if (phase_pos < 0.75f) {
            triangle_table[i] = 2.0f - 4.0f * phase_pos;
        } else {
            triangle_table[i] = 4.0f * phase_pos - 4.0f;
        }
    }
}




// ============================================================
// AUTO-TUNE
// ============================================================

float process_autotune(float input_freq) {
    // Clamp to valid range
    if (input_freq <= piano_keys_frequencies[0]) {
        return piano_keys_frequencies[0];
    }
    if (input_freq >= piano_keys_frequencies[NUM_PIANO_KEYS - 1]) {
        return piano_keys_frequencies[NUM_PIANO_KEYS - 1];
    }
    
    // Binary search to find the interval
    int low = 0;
    int high = NUM_PIANO_KEYS - 1;
    
    while (high - low > 1) {
        int mid = (low + high) / 2;
        if (piano_keys_frequencies[mid] < input_freq) {
            low = mid;
        } else {
            high = mid;
        }
    }
    
    // Now input_freq is between low and high
    // Return whichever is closer
    float dist_low = input_freq - piano_keys_frequencies[low];
    float dist_high = piano_keys_frequencies[high] - input_freq;
    
    if (dist_low < dist_high) {
        return piano_keys_frequencies[low];
    } else {
        return piano_keys_frequencies[high];
    }
}       

void reset_autotune(void) {
    current_freq = A4_FREQUENCY;
    target_freq = A4_FREQUENCY;
    last_input_freq = A4_FREQUENCY;
}

// ============================================================
// WAVEFORM GENERATION
// ============================================================

void set_frequency(float frequency) {
    float cycles_per_sample = frequency / (float)SAMPLE_RATE;
    phase_increment_main = (uint32_t)(cycles_per_sample * PHASE_MAX);
}

void set_frequency_multi(float frequency) {
    float cycles_per_sample = frequency / (float)SAMPLE_RATE;
    phase_increment_main = (uint32_t)(cycles_per_sample * PHASE_MAX);
    
    float cycles_detune1 = (frequency * 1.003f) / (float)SAMPLE_RATE;
    phase_increment_detune1 = (uint32_t)(cycles_detune1 * PHASE_MAX);
    
    float cycles_detune2 = (frequency * 0.997f) / (float)SAMPLE_RATE;
    phase_increment_detune2 = (uint32_t)(cycles_detune2 * PHASE_MAX);
}

void set_frequency_octave(float frequency) {
    float cycles_per_sample = frequency / (float)SAMPLE_RATE;
    phase_increment_main = (uint32_t)(cycles_per_sample * PHASE_MAX);
    
    float cycles_octave = (frequency * 2.0f) / (float)SAMPLE_RATE;
    phase_increment_octave = (uint32_t)(cycles_octave * PHASE_MAX);
}

float generate_sample_from_table(float* table, uint32_t* phase, uint32_t phase_inc) {
    *phase += phase_inc;
    uint8_t table_index = (*phase >> 24) & 0xFF;
    return table[table_index];
}

float generate_sample(void) {
    float* table;
    switch (current_waveform) {
        case WAVEFORM_SINE:     table = sine_table; break;
        case WAVEFORM_SQUARE:   table = square_table; break;
        case WAVEFORM_SAWTOOTH: table = sawtooth_table; break;
        case WAVEFORM_TRIANGLE: table = triangle_table; break;
        default:                table = sine_table;
    }
    
    return generate_sample_from_table(table, &phase_main, phase_increment_main);
}

// ============================================================
// EFFECTS
// ============================================================

float apply_distortion(float input_sample) {
    float drive = 1.0f + DISTORTION_AMOUNT * (DISTORTION_DRIVE_MAX - 1.0f);
    float amplified = input_sample * drive;
    float clipped = tanhf(amplified);
    return clipped * DISTORTION_AMOUNT;
}

float apply_tremolo(float sample, float rate, float depth) {
    tremolo_phase += (rate * 2.0f * M_PI) / (float)SAMPLE_RATE;
    if (tremolo_phase > 2.0f * M_PI) {
        tremolo_phase -= 2.0f * M_PI;
    }
    
    float modulation = 1.0f - depth * (0.5f + 0.5f * sinf(tremolo_phase));
    return sample * modulation;
}

float apply_vibrato(float frequency, float rate, float depth) {
    vibrato_phase += (rate * 2.0f * M_PI) / (float)SAMPLE_RATE;
    if (vibrato_phase > 2.0f * M_PI) {
        vibrato_phase -= 2.0f * M_PI;
    }
    
    float vibrato_amount = depth * sinf(vibrato_phase);
    return frequency + vibrato_amount;
}

float apply_lowpass_filter(float sample) {
    filter_buffer[filter_index] = sample;
    filter_index = (filter_index + 1) % FILTER_SIZE;
    
    float sum = 0.0f;
    for (int i = 0; i < FILTER_SIZE; i++) {
        sum += filter_buffer[i];
    }
    
    return sum / (float)FILTER_SIZE;
}

// ============================================================
// PROFILE MANAGEMENT
// ============================================================

void set_profile(SoundProfile profile) {
    if (profile >= NUM_PROFILES) return;
    
    current_profile = profile;
    reset_autotune();
    tremolo_phase = 0.0f;
    vibrato_phase = 0.0f;
    
    for (int i = 0; i < FILTER_SIZE; i++) {
        filter_buffer[i] = 0.0f;
    }
    filter_index = 0;
    
    switch (profile) {
        case PROFILE_AUTOTUNE:
            current_waveform = WAVEFORM_SINE;
            break;
        case PROFILE_DISTORTION:
            current_waveform = WAVEFORM_SAWTOOTH;
            break;
        case PROFILE_AMBIENT:
            current_waveform = WAVEFORM_SINE;
            break;
    }
}

// ============================================================
// PWM
// ============================================================

void setup_pwm(void) {
    gpio_set_function(PWM_OUTPUT_PIN, GPIO_FUNC_PWM);
    pwm_slice_num = pwm_gpio_to_slice_num(PWM_OUTPUT_PIN);
    pwm_set_wrap(pwm_slice_num, PWM_WRAP_VALUE);
    pwm_set_gpio_level(PWM_OUTPUT_PIN, PWM_WRAP_VALUE / 2);
    pwm_set_enabled(pwm_slice_num, true);
}

void output_sample_to_pwm(int16_t sample, float percent) {
    uint16_t duty_cycle = (uint16_t)(sample + 32768);
    pwm_set_gpio_level(PWM_OUTPUT_PIN, percent * duty_cycle);
}



// ============================================================
// FREQUENCY INPUT
// ============================================================

float get_frequency(void) {
    
    // // Add smoothing to reduce noise
    // static float smoothed_freq = 440.0f;
    // float alpha = 0.2f;  // Smoothing factor
    // smoothed_freq = smoothed_freq * (1.0f - alpha) + freq * alpha;
    
    // // Clamp to reasonable range
    // if (smoothed_freq < 110.0f) smoothed_freq = 110.0f;
    // if (smoothed_freq > 4186.0f) smoothed_freq = 4186.0f;
    
    return freq;
}

// ============================================================
// AUDIO PROCESSING
// ============================================================
void process_one_audio_sample(void) {
    input_frequency = get_frequency();
    float sample_float = 0.0f;
    
    // PROFILE 0: AUTO-TUNE
    if (current_profile == PROFILE_AUTOTUNE) {
        float processed_freq = process_autotune(input_frequency);
        set_frequency(processed_freq);
        sample_float = generate_sample();
    }
    
    // PROFILE 1: DISTORTION 
    else if (current_profile == PROFILE_DISTORTION) {
        set_frequency(input_frequency);
        current_waveform = WAVEFORM_SAWTOOTH;
        sample_float = generate_sample();
        
        // Simple hard clipping instead of tanh
        if (sample_float > 0.3f) {
            sample_float = 0.3f + (sample_float - 0.3f) * 0.1f;
        }
        if (sample_float < -0.3f) {
            sample_float = -0.3f + (sample_float + 0.3f) * 0.1f;
        }
        sample_float *= 2.0f;
    }
    
    // PROFILE 2: AMBIENT (Simplified - no vibrato, no filter)
    else {
        set_frequency_multi(input_frequency);
        
        float sample_main = generate_sample_from_table(sine_table, 
                                                       &phase_main, 
                                                       phase_increment_main);
        float sample_detune1 = generate_sample_from_table(sine_table, 
                                                          &phase_detune1, 
                                                          phase_increment_detune1);
        float sample_detune2 = generate_sample_from_table(sine_table, 
                                                          &phase_detune2, 
                                                          phase_increment_detune2);
        
        sample_float = (sample_main * 0.5f + 
                       sample_detune1 * 0.25f + 
                       sample_detune2 * 0.25f);
    }
    
    // Clamp
    if (sample_float > 1.0f) sample_float = 1.0f;
    if (sample_float < -1.0f) sample_float = -1.0f;
    
    // Convert and output
    int16_t sample_int16 = (int16_t)(sample_float * 32767.0f);
    output_sample_to_pwm(sample_int16, percent);

    // // TEST: Output constant value
    // int16_t sample_int16 = 16384;  // 50% of max (1.65V)
    // output_sample_to_pwm(sample_int16);

    
    last_sample_output = sample_int16;
    // Update counters
    total_samples_generated++;
    sample_counter++;
    
    if (sample_counter >= 4410) {
        sample_counter = 0;
        debug_print_flag = true;
    }
}


// ============================================================
// TIMER CALLBACK
// ============================================================

bool audio_timer_callback(struct repeating_timer *t) {
    for (int i = 0; i < SAMPLES_PER_CALLBACK; i++) {
        process_one_audio_sample();
    }
    return true;
}

// ============================================================
// SETUP
// ============================================================

void setup_audio_processing(void) {
    init_wavetables();
    setup_pwm();
    set_profile(PROFILE_AUTOTUNE);
    set_frequency(A4_FREQUENCY);
}

void setup_audio_timer(void) {
    static struct repeating_timer timer;
    
    if (!add_repeating_timer_us(-CALLBACK_PERIOD_US, audio_timer_callback, NULL, &timer)) {
        printf("ERROR: Timer failed\n");
    } else {
        printf("Audio timer started: %d Hz\n", SAMPLE_RATE);
    }
}


// int main(void) {
//     stdio_init_all();
//     sleep_ms(2000);
    
//     printf("\n========================================================\n");
//     printf("  Digital Theremin - 3 Sound Profiles\n");
//     printf("========================================================\n\n");
    
//     // Initialize LED
//     gpio_init(DEBUG_LED_PIN);
//     gpio_set_dir(DEBUG_LED_PIN, GPIO_OUT);
    
//     // Setup audio
//     setup_audio_processing();
//     setup_audio_timer();
//     setup_profile_buttons();
    
//     printf("Audio processing active!\n");
//     printf("Press 0/1/2 to switch profiles, 's' for stats\n\n");
    
//     // Main loop
//     uint32_t last_debug_time = 0;
    
//     while (true) {
//         // Auto-print every 1 second with frequency and sample info
//         static uint32_t last_auto_print = 0;
//         uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        
//         if (now_ms - last_auto_print > 1000) {  // Every 1 second
//             last_auto_print = now_ms;
            
//             // Get current frequencies
//             float current_input = get_frequency();
//             float current_output = 0.0f;
//             // Calculate output based on profile
//             if (current_profile == PROFILE_AUTOTUNE) {
//                 current_output = process_autotune(current_input);
//                 printf("[Profile 0: Auto-Tune] Input: %.2f Hz → Output: %.2f Hz | Samples: %lu | Time: %.1fs\n",
//                        current_input,
//                        current_output,
//                        total_samples_generated,
//                        (float)total_samples_generated / (float)SAMPLE_RATE);
//             } 
//             else if (current_profile == PROFILE_DISTORTION) {
//                 current_output = current_input;
//                 printf("[Profile 1: Distortion] Input: %.2f Hz → Output: %.2f Hz | Sample: %d | Samples: %lu | Time: %.1fs\n",
//                        current_input,
//                        current_output,
//                        last_sample_output,
//                        total_samples_generated,
//                        (float)total_samples_generated / (float)SAMPLE_RATE);
//             }
//             else {  // PROFILE_AMBIENT
//                 current_output = current_input;
//                 printf("[Profile 2: Ambient] Input: %.2f Hz → Output: %.2f Hz | Sample: %d | Samples: %lu | Time: %.1fs\n",
//                        current_input,
//                        current_output,
//                        last_sample_output,
//                        total_samples_generated,
//                        (float)total_samples_generated / (float)SAMPLE_RATE);
//             }
//         }
        
//         // Handle input
//         int c = getchar_timeout_us(0);
//         if (c != PICO_ERROR_TIMEOUT) {
//             switch (c) {
//                 case '0': 
//                     set_profile(PROFILE_AUTOTUNE); 
//                     break;
//                 case '1': 
//                     set_profile(PROFILE_DISTORTION); 
//                     break;
//                 case '2': 
//                     set_profile(PROFILE_AMBIENT); 
//                     break;
//                 case 's':
//                 case 'S':
//                     printf("\n========================================\n");
//                     printf("  STATISTICS\n");
//                     printf("========================================\n");
//                     printf("Total samples: %lu\n", total_samples_generated);
//                     printf("Running time:  %.2f seconds\n", 
//                            (float)total_samples_generated / (float)SAMPLE_RATE);
//                     printf("Current profile: %d ", current_profile);
//                     switch(current_profile) {
//                         case 0: printf("(Auto-Tune)\n"); break;
//                         case 1: printf("(Distortion)\n"); break;
//                         case 2: printf("(Ambient)\n"); break;
//                         default: printf("(Unknown)\n"); break;
//                     }
//                     printf("Current input frequency: %.2f Hz\n", get_frequency());
//                     printf("Last sample output: %d (int16)\n", last_sample_output);
//                     printf("========================================\n\n");
//                     break;
//             }
//         }
        
//         // Toggle debug LED
//         uint32_t now = to_ms_since_boot(get_absolute_time());
//         if (debug_print_flag && (now - last_debug_time > 100)) {
//             debug_print_flag = false;
//             last_debug_time = now;
//             gpio_put(DEBUG_LED_PIN, !gpio_get(DEBUG_LED_PIN));
//         }
        
//         sleep_ms(10);
//     }
    
//     return 0;
// }

