// sound_profiles.c
// Sound profile processing for theremin using Pico SDK
// Integrates auto-tune and waveform generation

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "pico/stdlib.h"        // Pico SDK standard library
#include "hardware/adc.h"       // RP2040/RP2350 ADC library
#include "pico/time.h"          // Time functions
#include "../include/autotune.h"
#include "../include/waveform.h"

// ============================================================
// CONFIGURATION
// ============================================================

// ADC Configuration
#define ADC_CHANNEL 0              // ADC channel for frequency input
#define ADC_PIN 26                 // GPIO 26 = ADC0 - UPDATE THIS IF NEEDED
#define ADC_VREF 3.3f              // ADC reference voltage
#define ADC_MAX_VALUE 4095         // 12-bit ADC (0-4095)

// Audio Configuration
#define AUDIO_BUFFER_SIZE 256      // Number of samples per buffer
#define SAMPLE_RATE 44100          // Audio sample rate

// Frequency Range
#define MIN_FREQUENCY 65.41f       // C2
#define MAX_FREQUENCY 2093.0f      // C7

// Profile Configuration
#define PROFILE_AUTOTUNE 0         // Profile index for auto-tune
#define AUTOTUNE_STRENGTH 1.0f     // 100% correction
#define AUTOTUNE_GLIDE 0.3f        // Smooth glide rate

// ============================================================
// GLOBAL VARIABLES
// ============================================================

// Audio output buffer
int16_t audio_buffer[AUDIO_BUFFER_SIZE];

// Current audio buffer index
uint16_t buffer_index = 0;

// Oscillator for waveform generation
Oscillator oscillator;

// Current profile (0 = auto-tune, others would be different effects)
uint8_t current_profile = PROFILE_AUTOTUNE;

// ============================================================
// FUNCTION DECLARATIONS
// ============================================================

void setup_adc(void);
float read_frequency_from_antenna(void);
float adc_value_to_frequency(uint16_t adc_value);
void process_audio_sample(void);
void send_buffer_to_partner(void);

// ============================================================
// MAIN FUNCTION (Pico SDK style)
// ============================================================

int main() {
    // ════════════════════════════════════════════════════════
    // INITIALIZATION
    // ════════════════════════════════════════════════════════
    
    // Initialize standard I/O (enables printf)
    stdio_init_all();
    
    // Small delay to let USB serial stabilize
    sleep_ms(2000);
    
    // Print startup banner
    printf("\n");
    printf("========================================\n");
    printf("  Theremin Sound Profiles System\n");
    printf("  Auto-Tune Profile Active\n");
    printf("========================================\n");
    printf("\n");
    printf("Initializing...\n");
    
    // STEP 1: Initialize ADC for reading frequency
    setup_adc();
    printf("✓ ADC initialized\n");
    
    // STEP 2: Initialize auto-tune system
    autotune_init();
    printf("✓ Auto-tune initialized\n");
    
    // STEP 3: Initialize waveform generation
    waveform_init();
    printf("✓ Waveform tables generated\n");
    
    // STEP 4: Initialize oscillator
    // Start with sine wave (Profile 0 uses sine)
    oscillator_init(&oscillator, WAVEFORM_SINE);
    oscillator_set_frequency(&oscillator, 440.0f);  // Start at A4
    printf("✓ Oscillator initialized\n");
    
    printf("\n");
    printf("Setup complete! Starting audio generation...\n");
    printf("========================================\n");
    printf("\n");
    
    // ════════════════════════════════════════════════════════
    // MAIN LOOP - Generate Audio Continuously
    // ════════════════════════════════════════════════════════
    
    while (true) {
        // Fill the audio buffer with processed samples
        // Each buffer contains 256 samples (about 5.8ms of audio at 44.1kHz)
        for (buffer_index = 0; buffer_index < AUDIO_BUFFER_SIZE; buffer_index++) {
            // Generate one audio sample with sound profile processing
            process_audio_sample();
        }
        
        // Buffer is full - send to partner for PWM conversion
        send_buffer_to_partner();
        
        // In a real implementation, you'd wait for partner to finish
        // processing before filling the next buffer (double buffering)
        // For now, we just continue immediately
    }
    
    return 0;
}

// ============================================================
// ADC SETUP
// ============================================================

void setup_adc(void) {
    // Initialize the ADC hardware on the RP2350
    // The ADC reads voltage from your partner's frequency-to-voltage circuit
    
    // Initialize the ADC subsystem
    adc_init();
    
    // Configure GPIO pin for ADC input
    // GPIO 26 = ADC channel 0 (ADC0)
    // GPIO 27 = ADC channel 1 (ADC1)
    // GPIO 28 = ADC channel 2 (ADC2)
    // GPIO 29 = ADC channel 3 (ADC3)
    // 
    // UPDATE PIN NUMBER IF YOUR HARDWARE IS DIFFERENT
    adc_gpio_init(ADC_PIN);
    
    // Select which ADC channel to read from
    // This must match the channel corresponding to your GPIO pin
    adc_select_input(ADC_CHANNEL);
}

// ============================================================
// READ FREQUENCY FROM ANTENNA
// ============================================================

float read_frequency_from_antenna(void) {
    // This function reads the frequency value from your partner's circuit
    // 
    // Signal flow:
    // Hand position → Antenna capacitance → Oscillator frequency →
    // Frequency-to-voltage converter (your partner's EE circuit) →
    // Voltage (0-3.3V) → ADC → Digital value (0-4095) → 
    // Frequency (65-2093 Hz) [this function]
    
    // STEP 1: Read raw ADC value
    // The ADC samples the voltage and returns a 12-bit number (0-4095)
    // 0 = 0V, 4095 = 3.3V (reference voltage)
    uint16_t adc_value = adc_read();
    
    // STEP 2: Convert ADC value to frequency
    // Your partner's circuit outputs voltage proportional to pitch
    // We convert that voltage (represented as ADC value) to frequency in Hz
    float frequency = adc_value_to_frequency(adc_value);
    
    return frequency;
}

// ============================================================
// ADC VALUE TO FREQUENCY CONVERSION
// ============================================================

float adc_value_to_frequency(uint16_t adc_value) {
    // Convert ADC reading (0-4095) to frequency (65-2093 Hz)
    //
    // We use EXPONENTIAL mapping for musical pitch
    // This is important! Linear mapping would not sound musical.
    //
    // Why exponential?
    // - Musical pitch is logarithmic (each octave doubles frequency)
    // - Equal hand movements should give equal musical intervals
    // - This matches how humans perceive pitch
    //
    // Formula: frequency = MIN_FREQ × 2^(normalized_value × octaves)
    // 
    // Our range:
    // - C2 (65.41 Hz) to C7 (2093 Hz)
    // - That's 5 octaves (32x frequency range)
    // - log2(2093 / 65.41) = log2(32) = 5 octaves
    
    // STEP 1: Normalize ADC value to 0.0 - 1.0 range
    // 0 → 0.0 (minimum)
    // 2047 → 0.5 (middle)
    // 4095 → 1.0 (maximum)
    float normalized = (float)adc_value / (float)ADC_MAX_VALUE;
    
    // STEP 2: Calculate frequency range in octaves
    // Our range spans 5 octaves (C2 to C7)
    float octaves = 5.0f;
    
    // STEP 3: Calculate frequency using exponential mapping
    // This creates a musically-correct pitch response
    // 
    // Examples:
    // normalized = 0.0  → freq = 65.41 × 2^0 = 65.41 Hz (C2)
    // normalized = 0.2  → freq = 65.41 × 2^1 = 130.8 Hz (C3)
    // normalized = 0.4  → freq = 65.41 × 2^2 = 261.6 Hz (C4 - middle C)
    // normalized = 0.6  → freq = 65.41 × 2^3 = 523.2 Hz (C5)
    // normalized = 0.8  → freq = 65.41 × 2^4 = 1046 Hz (C6)
    // normalized = 1.0  → freq = 65.41 × 2^5 = 2093 Hz (C7)
    float frequency = MIN_FREQUENCY * powf(2.0f, normalized * octaves);
    
    // STEP 4: Clamp to valid range (safety check)
    // In case of ADC noise or errors, ensure we stay in bounds
    if (frequency < MIN_FREQUENCY) frequency = MIN_FREQUENCY;
    if (frequency > MAX_FREQUENCY) frequency = MAX_FREQUENCY;
    
    return frequency;
}

// ============================================================
// PROCESS ONE AUDIO SAMPLE
// ============================================================

void process_audio_sample(void) {
    // This is the heart of the sound profile system!
    // This function is called 44,100 times per second (once per audio sample)
    // It processes the raw frequency through the active sound profile
    //
    // Current implementation: Auto-tune profile only
    // Future: Will handle all 4 profiles (auto-tune, reverb, distortion, delay)
    
    // ════════════════════════════════════════════════════════
    // STEP 1: READ INPUT FREQUENCY
    // ════════════════════════════════════════════════════════
    
    // Read the current frequency from the antenna via ADC
    // This is the "raw" frequency based on hand position
    // It might be slightly off-pitch (e.g., 442.3 Hz instead of 440 Hz)
    float raw_frequency = read_frequency_from_antenna();
    
    // ════════════════════════════════════════════════════════
    // STEP 2: APPLY SOUND PROFILE PROCESSING
    // ════════════════════════════════════════════════════════
    
    float corrected_frequency;
    
    // Check which profile is currently active
    if (current_profile == PROFILE_AUTOTUNE) {
        // ────────────────────────────────────────────────────
        // PROFILE 0: AUTO-TUNE
        // ────────────────────────────────────────────────────
        // This profile corrects pitch to the nearest musical note
        // Makes the theremin easier to play in tune
        
        // Apply auto-tune correction
        corrected_frequency = process_autotune(
            raw_frequency,         // Input: Raw frequency from antenna
            AUTOTUNE_STRENGTH,     // 1.0 = 100% correction (perfect pitch)
            AUTOTUNE_GLIDE        // 0.3 = smooth glide rate (no clicks)
        );
        
        // Debug output (prints every 0.1 seconds)
        // Shows raw vs corrected frequency
        // Remove this in production for better performance
        static int debug_counter = 0;
        if (debug_counter++ >= 4410) {  // 4410 samples = 0.1 seconds at 44.1kHz
            printf("Profile 0 (Auto-Tune) | Raw: %.2f Hz → Corrected: %.2f Hz\n", 
                   raw_frequency, corrected_frequency);
            debug_counter = 0;
        }
        
    } else {
        // ────────────────────────────────────────────────────
        // OTHER PROFILES: NO AUTO-TUNE
        // ────────────────────────────────────────────────────
        // Profiles 1, 2, 3 use natural pitch (no correction)
        // They apply other effects instead:
        // - Profile 1: Reverb
        // - Profile 2: Distortion
        // - Profile 3: Delay
        
        corrected_frequency = raw_frequency;  // Use raw frequency directly
    }
    
    // ════════════════════════════════════════════════════════
    // STEP 3: SET OSCILLATOR FREQUENCY
    // ════════════════════════════════════════════════════════
    
    // Update the oscillator to generate audio at the corrected frequency
    // This calculates the phase increment needed to produce this frequency
    oscillator_set_frequency(&oscillator, corrected_frequency);
    
    // ════════════════════════════════════════════════════════
    // STEP 4: GENERATE WAVEFORM SAMPLE
    // ════════════════════════════════════════════════════════
    
    // Generate one sample of the selected waveform
    // Returns a float value from -1.0 to +1.0
    // This represents the amplitude of the wave at this instant
    float sample_float = oscillator_generate_sample(&oscillator);
    
    // ════════════════════════════════════════════════════════
    // STEP 5: APPLY ADDITIONAL EFFECTS (FUTURE)
    // ════════════════════════════════════════════════════════
    
    // TODO: Add effect processing based on active profile:
    //
    // if (profile has distortion):
    //     sample_float = apply_distortion(sample_float, amount);
    //
    // if (profile has delay):
    //     sample_float = apply_delay(sample_float, time, feedback, mix);
    //
    // if (profile has reverb):
    //     sample_float = apply_reverb(sample_float, feedback, mix);
    
    // ════════════════════════════════════════════════════════
    // STEP 6: CONVERT TO INTEGER FORMAT
    // ════════════════════════════════════════════════════════
    
    // Convert float sample (-1.0 to +1.0) to 16-bit integer
    // This is the format your partner needs for PWM conversion
    //
    // Scaling:
    // -1.0 → -32768 (maximum negative amplitude)
    //  0.0 →      0 (silence/center)
    // +1.0 → +32767 (maximum positive amplitude)
    int16_t sample_int16 = (int16_t)(sample_float * 32767.0f);
    
    // ════════════════════════════════════════════════════════
    // STEP 7: STORE IN OUTPUT BUFFER
    // ════════════════════════════════════════════════════════
    
    // Store the processed sample in the output buffer
    // Once the buffer is full (256 samples), we send it to your partner
    audio_buffer[buffer_index] = sample_int16;
}

// ============================================================
// SEND BUFFER TO PARTNER
// ============================================================

void send_buffer_to_partner(void) {
    // This function sends the filled audio buffer to your partner's
    // PWM conversion system
    //
    // ════════════════════════════════════════════════════════
    // WHAT YOUR PARTNER RECEIVES:
    // ════════════════════════════════════════════════════════
    //
    // 1. AUDIO BUFFER (audio_buffer[256])
    //    - Array of 256 samples
    //    - Each sample is int16_t (-32768 to +32767)
    //    - Represents amplitude at each instant
    //    - 256 samples = 5.8ms of audio at 44.1kHz
    //
    // 2. WAVEFORM TYPE (oscillator.waveform_type)
    //    - Single value: 0, 1, 2, or 3
    //    - 0 = Sine, 1 = Square, 2 = Sawtooth, 3 = Triangle
    //    - For display/informational purposes
    //
    // ════════════════════════════════════════════════════════
    // WHAT YOUR PARTNER DOES WITH IT:
    // ════════════════════════════════════════════════════════
    //
    // For each sample in the buffer:
    //   1. Convert sample value to PWM duty cycle
    //      duty_cycle = (sample + 32768) / 65536
    //      (Maps -32768 to +32767 → 0% to 100% duty cycle)
    //   
    //   2. Set PWM output
    //      pwm_set_duty_cycle(duty_cycle)
    //   
    //   3. PWM pin outputs rapid pulses
    //   
    //   4. RC filter smooths PWM → analog voltage
    //   
    //   5. Analog voltage → volume control → amplifier → speaker
    //
    // ════════════════════════════════════════════════════════
    // INTERFACE IMPLEMENTATION
    // ════════════════════════════════════════════════════════
    
    // PLACEHOLDER: Replace this section with your actual interface
    //
    // You need to coordinate with your partner on HOW to send the data.
    // Common methods:
    
    // ────────────────────────────────────────────────────────
    // METHOD A: SHARED MEMORY (Same microcontroller)
    // ────────────────────────────────────────────────────────
    // If you and your partner are both writing code for the same RP2350:
    //
    // extern int16_t* partner_audio_buffer;
    // extern uint8_t* partner_waveform_type;
    // extern volatile bool* partner_buffer_ready;
    //
    // memcpy(partner_audio_buffer, audio_buffer, sizeof(audio_buffer));
    // *partner_waveform_type = oscillator.waveform_type;
    // *partner_buffer_ready = true;
    
    // ────────────────────────────────────────────────────────
    // METHOD B: FUNCTION CALL (She provides an API)
    // ────────────────────────────────────────────────────────
    // If your partner provides a function to receive audio:
    //
    // extern void partner_receive_audio(int16_t* buffer, uint16_t size, 
    //                                   uint8_t waveform);
    // 
    // partner_receive_audio(audio_buffer, AUDIO_BUFFER_SIZE, 
    //                      oscillator.waveform_type);
    
    // ────────────────────────────────────────────────────────
    // METHOD C: DIRECT PWM CONTROL (You handle PWM)
    // ────────────────────────────────────────────────────────
    // If YOU are responsible for PWM output:
    //
    // #include "hardware/pwm.h"
    // 
    // #define PWM_PIN 15  // UPDATE THIS
    // 
    // for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
    //     // Convert sample to duty cycle (0-65535)
    //     uint16_t duty = (uint16_t)(audio_buffer[i] + 32768);
    //     
    //     // Set PWM duty cycle
    //     pwm_set_gpio_level(PWM_PIN, duty);
    //     
    //     // Wait for sample period (22.7 microseconds at 44.1kHz)
    //     sleep_us(23);
    // }
    
    // ────────────────────────────────────────────────────────
    // METHOD D: HARDWARE INTERFACE (SPI/I2C/DMA)
    // ────────────────────────────────────────────────────────
    // If using hardware interface:
    //
    // #include "hardware/spi.h"
    // 
    // spi_write_blocking(spi0, (uint8_t*)audio_buffer, 
    //                    AUDIO_BUFFER_SIZE * sizeof(int16_t));
    
    // ════════════════════════════════════════════════════════
    // CURRENT IMPLEMENTATION: DEBUG OUTPUT
    // ════════════════════════════════════════════════════════
    
    // For now, just print that we're sending data (for testing)
    // Get current time in milliseconds
    static uint64_t last_print_time = 0;
    uint64_t current_time = to_ms_since_boot(get_absolute_time());
    
    if (current_time - last_print_time >= 1000) {  // Print once per second
        printf("→ Buffer ready: %d samples | Waveform: ", AUDIO_BUFFER_SIZE);
        
        // Print waveform name instead of number
        switch (oscillator.waveform_type) {
            case WAVEFORM_SINE:
                printf("Sine");
                break;
            case WAVEFORM_SQUARE:
                printf("Square");
                break;
            case WAVEFORM_SAWTOOTH:
                printf("Sawtooth");
                break;
            case WAVEFORM_TRIANGLE:
                printf("Triangle");
                break;
            default:
                printf("Unknown");
                break;
        }
        
        printf("\n");
        last_print_time = current_time;
    }
    
    // ════════════════════════════════════════════════════════
    // TIMING CONSIDERATIONS (FOR PRODUCTION)
    // ════════════════════════════════════════════════════════
    
    // In production, you need to handle timing carefully:
    //
    // 1. DOUBLE BUFFERING:
    //    - Use two buffers (A and B)
    //    - While partner processes buffer A, you fill buffer B
    //    - Swap buffers when both are ready
    //    - Prevents audio glitches
    //
    // 2. SYNCHRONIZATION:
    //    - Wait for partner to signal "ready for next buffer"
    //    - Use flags, interrupts, or semaphores
    //    - Ensure no buffer overruns or underruns
    //
    // 3. SAMPLE RATE ACCURACY:
    //    - Must deliver samples at exactly 44,100 Hz
    //    - Use timers or DMA for precise timing
    //    - Jitter causes audio quality issues
}