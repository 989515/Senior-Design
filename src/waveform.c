// waveform.c
// Implementation of waveform generation

#include "../include/waveform.h"
#include <math.h>

// ============================================================
// GLOBAL WAVETABLES
// ============================================================

// These tables store one complete cycle of each waveform
// We pre-calculate them once and then just look up values
float sine_table[WAVETABLE_SIZE];
float square_table[WAVETABLE_SIZE];
float sawtooth_table[WAVETABLE_SIZE];
float triangle_table[WAVETABLE_SIZE];

// ============================================================
// INITIALIZATION
// ============================================================

void waveform_init(void) {
    // Fill all the wavetables with pre-calculated values
    // This is done once at startup for efficiency
    
    for (int i = 0; i < WAVETABLE_SIZE; i++) {
        // Calculate position in the cycle (0.0 to 1.0)
        float position = (float)i / (float)WAVETABLE_SIZE;
        
        // SINE WAVE
        // Use the standard sine function
        // Position 0.0 to 1.0 maps to 0 to 2π radians
        float angle = position * 2.0f * M_PI;
        sine_table[i] = sinf(angle);
        
        // SQUARE WAVE
        // First half of cycle = +1.0, second half = -1.0
        if (i < WAVETABLE_SIZE / 2) {
            square_table[i] = 1.0f;
        } else {
            square_table[i] = -1.0f;
        }
        
        // SAWTOOTH WAVE
        // Ramps from -1.0 to +1.0 linearly
        sawtooth_table[i] = (position * 2.0f) - 1.0f;
        
        // TRIANGLE WAVE
        // Ramps up to halfway, then ramps down
        if (i < WAVETABLE_SIZE / 2) {
            // First half: ramp up from -1.0 to +1.0
            triangle_table[i] = (position * 4.0f) - 1.0f;
        } else {
            // Second half: ramp down from +1.0 to -1.0
            triangle_table[i] = 3.0f - (position * 4.0f);
        }
    }
}

// ============================================================
// OSCILLATOR FUNCTIONS
// ============================================================

void oscillator_init(Oscillator* osc, WaveformType type) {
    // Initialize an oscillator to default state
    osc->phase = 0;              // Start at beginning of waveform
    osc->phase_increment = 0;    // No frequency yet
    osc->waveform_type = type;   // Set waveform type
}

void oscillator_set_frequency(Oscillator* osc, float frequency) {
    // Calculate the phase increment for this frequency
    // 
    // Phase accumulator explanation:
    // - We use a 32-bit integer (0 to 4,294,967,296) to represent phase
    // - 0 = start of waveform, 4,294,967,296 = end of waveform
    // - Each sample, we add phase_increment to phase
    // - When phase wraps around, we've completed one cycle
    //
    // Formula: phase_increment = (frequency / sample_rate) × 2^32
    // 
    // Example: For 440 Hz at 44,100 Hz sample rate:
    //   phase_increment = (440 / 44100) × 2^32
    //   phase_increment = 0.00998 × 4,294,967,296
    //   phase_increment = 42,854,614
    //
    // This means we advance through 42,854,614 "steps" of the 2^32 total
    // steps each sample, completing exactly 440 cycles per second!
    
    float cycles_per_sample = frequency / (float)SAMPLE_RATE;
    osc->phase_increment = (uint32_t)(cycles_per_sample * PHASE_SCALE);
}

void oscillator_set_waveform(Oscillator* osc, WaveformType type) {
    // Change the waveform type
    osc->waveform_type = type;
}

float oscillator_generate_sample(Oscillator* osc) {
    // Generate one audio sample from the oscillator
    
    // STEP 1: Convert phase (32-bit) to wavetable index (0-255)
    // We take the top 8 bits of the 32-bit phase
    // This gives us a value from 0 to 255
    uint8_t table_index = (osc->phase >> 24) & 0xFF;
    
    // STEP 2: Look up the sample value from the appropriate table
    float sample = 0.0f;
    
    switch (osc->waveform_type) {
        case WAVEFORM_SINE:
            sample = sine_table[table_index];
            break;
            
        case WAVEFORM_SQUARE:
            sample = square_table[table_index];
            break;
            
        case WAVEFORM_SAWTOOTH:
            sample = sawtooth_table[table_index];
            break;
            
        case WAVEFORM_TRIANGLE:
            sample = triangle_table[table_index];
            break;
            
        default:
            sample = 0.0f;  // Silence if unknown type
            break;
    }
    
    // STEP 3: Advance the phase for next sample
    // This automatically wraps around when it exceeds 2^32
    osc->phase += osc->phase_increment;
    
    // STEP 4: Return the sample
    return sample;
}