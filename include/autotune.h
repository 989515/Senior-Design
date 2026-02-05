// autotune.h
// Header file for auto-tune functionality
// Defines structures and function declarations

#ifndef AUTOTUNE_H
#define AUTOTUNE_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// CONSTANTS
// ============================================================

#define NUM_NOTES 60              // Total number of notes in our range
#define FIRST_NOTE_FREQ 65.41f    // C2 - lowest note
#define REFERENCE_A4 440.0f       // A4 - standard tuning reference
#define SAMPLE_RATE 44100         // Audio sample rate (Hz)

// ============================================================
// AUTO-TUNE STATE STRUCTURE
// ============================================================

// This structure holds all the state information for auto-tune
// It remembers where we are and where we're going
typedef struct {
    float current_freq;      // The frequency we're currently outputting
    float target_freq;       // The correct note we're gliding toward
    float last_input_freq;   // Previous input (to detect changes)
} AutoTuneState;

// ============================================================
// NOTE TABLE
// ============================================================

// Global array holding all 60 correct note frequencies
// Index 0 = C2 (65.41 Hz), Index 49 = A4 (440 Hz), Index 59 = C7 (2093 Hz)
extern float note_table[NUM_NOTES];

// ============================================================
// FUNCTION DECLARATIONS
// ============================================================

// Initialize the auto-tune system (call once at startup)
void autotune_init(void);

// Find the nearest correct musical note to the input frequency
// input_freq: The raw frequency from the antenna (Hz)
// Returns: The frequency of the nearest correct note (Hz)
float find_nearest_note(float input_freq);

// Process auto-tune correction on the input frequency
// input_freq: Raw frequency from antenna (Hz)
// strength: How much correction to apply (0.0 = none, 1.0 = full)
// glide_rate: How fast to transition (0.0 = slow, 1.0 = instant)
// Returns: The corrected frequency to use for waveform generation (Hz)
float process_autotune(float input_freq, float strength, float glide_rate);

// Reset auto-tune state (call when changing profiles)
void reset_autotune(void);

#endif // AUTOTUNE_H