// autotune.c
// Implementation of auto-tune functionality

#include "../include/autotune.h"
#include <math.h>
#include <stdlib.h>

// ============================================================
// GLOBAL VARIABLES
// ============================================================

// The note table stores all 60 correct note frequencies
float note_table[NUM_NOTES];

// piano keys without sharps (white keys only)
const float piano_keys_frequencies[38] = {
    110.0000, 123.4708, 130.8128, 146.8324,
    164.8138, 174.6141, 195.9977, 220.0000, 246.9417, 261.6256,
    293.6648, 329.6276, 349.2282, 391.9954, 440.0000, 493.8833,
    523.2511, 587.3295, 659.2551, 698.4565, 783.9909, 880.0000,
    987.7666, 1046.502, 1174.659, 1318.510, 1396.913, 1567.982,
    1760.000, 1975.533, 2093.005, 2349.318, 2637.020, 2793.826,
    3135.963, 3520.000, 3951.066, 4186.009
};

// piano keys with sharps (black and white keys)
// const float original_piano_keys_frequencies[52] = {
//     27.50000, 30.86771, 32.70320, 36.70810, 41.20344, 43.65353,
//     48.99943, 55.00000, 61.73541, 65.40639, 73.41619, 82.40689,
//     87.30706, 97.99886, 110.0000, 123.4708, 130.8128, 146.8324,
//     164.8138, 174.6141, 195.9977, 220.0000, 246.9417, 261.6256,
//     293.6648, 329.6276, 349.2282, 391.9954, 440.0000, 493.8833,
//     523.2511, 587.3295, 659.2551, 698.4565, 783.9909, 880.0000,
//     987.7666, 1046.502, 1174.659, 1318.510, 1396.913, 1567.982,
//     1760.000, 1975.533, 2093.005, 2349.318, 2637.020, 2793.826,
//     3135.963, 3520.000, 3951.066, 4186.009
// };

// Auto-tune state - tracks where we are in the correction process
AutoTuneState autotune_state;

// ============================================================
// INITIALIZATION
// ============================================================

void autotune_init(void) {
    // Fill the note table with correct frequencies
    // We use the equal temperament formula:
    // frequency = 440 × 2^((note_number - 49) / 12)
    // 
    // Where note_number:
    //   0 = C2 (65.41 Hz)
    //   49 = A4 (440 Hz) - our reference
    //   59 = C7 (2093 Hz)
    
    for (int note_num = 0; note_num < NUM_NOTES; note_num++) {
        // Calculate how many semitones away from A4 this note is
        int semitones_from_a4 = note_num - 49;
        
        // Calculate the frequency using the equal temperament formula
        // 2^(semitones/12) gives us the frequency ratio
        float exponent = (float)semitones_from_a4 / 12.0f;
        float frequency = REFERENCE_A4 * powf(2.0f, exponent);
        
        // Store in the note table
        note_table[note_num] = frequency;
    }
    
    // Initialize auto-tune state to neutral values
    // Start at A4 (440 Hz) - middle of our range
    autotune_state.current_freq = REFERENCE_A4;
    autotune_state.target_freq = REFERENCE_A4;
    autotune_state.last_input_freq = 0.0f;
}

// ============================================================
// FIND NEAREST NOTE
// ============================================================

float find_nearest_note(float input_freq) {
    // This function finds which note in our table is closest
    // to the input frequency
    
    // Clamp input to valid range
    // If frequency is too low or too high, limit it to our range
    if (input_freq < FIRST_NOTE_FREQ) {
        input_freq = FIRST_NOTE_FREQ;
    }
    if (input_freq > note_table[NUM_NOTES - 1]) {
        input_freq = note_table[NUM_NOTES - 1];
    }
    
    // Initialize with impossible values
    float closest_distance = 99999.0f;  // Start with huge number
    float closest_freq = REFERENCE_A4;   // Default to A4
    
    // Loop through all 60 notes in our table
    for (int i = 0; i < NUM_NOTES; i++) {
        // Get the frequency of this note from the table
        float note_freq = note_table[i];
        
        // Calculate how far away this note is from the input
        // We use absolute value because we don't care if it's higher or lower
        float distance = fabsf(input_freq - note_freq);
        
        // Is this note closer than the best one we've found so far?
        if (distance < closest_distance) {
            // Yes! Update our "best match"
            closest_distance = distance;
            closest_freq = note_freq;
        }
    }
    
    // Return the frequency of the closest note we found
    return closest_freq;
}

// ============================================================
// PROCESS AUTO-TUNE
// ============================================================

float process_autotune(float input_freq, float strength, float glide_rate) {
    // This is the main auto-tune processing function
    // It takes raw frequency and returns corrected frequency
    
    // STEP 1: Check if input frequency changed significantly
    // We only recalculate the target note if the input changed by more than 1 Hz
    // This saves CPU cycles and prevents jitter
    float freq_change = fabsf(input_freq - autotune_state.last_input_freq);
    
    if (freq_change > 1.0f) {
        // Input changed! Find the new target note
        autotune_state.target_freq = find_nearest_note(input_freq);
        
        // Remember this input for next time
        autotune_state.last_input_freq = input_freq;
    }
    
    // STEP 2: Smoothly glide current frequency toward target
    // Instead of instantly jumping to the target (which would cause clicks),
    // we gradually move toward it
    
    // Calculate how far we are from the target
    float difference = autotune_state.target_freq - autotune_state.current_freq;
    
    // Move a fraction of that distance (controlled by glide_rate)
    // If glide_rate = 1.0, we snap instantly (difference × 1.0 = full jump)
    // If glide_rate = 0.1, we move 10% of the distance each sample
    float adjustment = difference * glide_rate;
    
    // Update our current frequency
    autotune_state.current_freq += adjustment;
    
    // STEP 3: Apply strength parameter
    // Strength controls how much correction to apply:
    // - strength = 0.0: No correction (use raw input)
    // - strength = 1.0: Full correction (use corrected frequency)
    // - strength = 0.5: Halfway between
    
    // Calculate the difference between raw and corrected
    float correction = autotune_state.current_freq - input_freq;
    
    // Apply only a portion of that correction based on strength
    float output_freq = input_freq + (correction * strength);
    
    // Return the final corrected frequency
    return output_freq;
}

// ============================================================
// RESET AUTO-TUNE
// ============================================================

void reset_autotune(void) {
    // Reset the auto-tune state to neutral
    // Call this when switching profiles to avoid glitches
    
    autotune_state.current_freq = REFERENCE_A4;
    autotune_state.target_freq = REFERENCE_A4;
    autotune_state.last_input_freq = 0.0f;
}