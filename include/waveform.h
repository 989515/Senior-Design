// waveform.h
// Header file for waveform generation
// Defines waveform types and oscillator structure

#ifndef WAVEFORM_H
#define WAVEFORM_H

#include <stdint.h>

// ============================================================
// CONSTANTS
// ============================================================

#define WAVETABLE_SIZE 256        // Number of samples in each waveform table
#define SAMPLE_RATE 44100         // Audio sample rate (Hz)
#define PHASE_SCALE 4294967296.0f // 2^32 for phase accumulator

// ============================================================
// WAVEFORM TYPES
// ============================================================

typedef enum {
    WAVEFORM_SINE = 0,      // Smooth, pure tone
    WAVEFORM_SQUARE = 1,    // Hollow, buzzy (odd harmonics)
    WAVEFORM_SAWTOOTH = 2,  // Bright, rich (all harmonics)
    WAVEFORM_TRIANGLE = 3   // Mellow, warm (weak odd harmonics)
} WaveformType;

// ============================================================
// OSCILLATOR STRUCTURE
// ============================================================

// The oscillator generates audio waveforms at a specified frequency
typedef struct {
    uint32_t phase;          // Current position in the waveform (0 to 2^32)
    uint32_t phase_increment; // How much to advance phase each sample
    WaveformType waveform_type; // Which waveform to generate
} Oscillator;

// ============================================================
// WAVETABLES
// ============================================================

// Pre-calculated waveform tables for fast lookup
// Each table has 256 samples representing one complete cycle
extern float sine_table[WAVETABLE_SIZE];
extern float square_table[WAVETABLE_SIZE];
extern float sawtooth_table[WAVETABLE_SIZE];
extern float triangle_table[WAVETABLE_SIZE];

// ============================================================
// FUNCTION DECLARATIONS
// ============================================================

// Initialize waveform system (call once at startup)
void waveform_init(void);

// Initialize an oscillator
void oscillator_init(Oscillator* osc, WaveformType type);

// Set the frequency of an oscillator
// osc: Pointer to the oscillator
// frequency: Desired frequency in Hz
void oscillator_set_frequency(Oscillator* osc, float frequency);

// Set the waveform type of an oscillator
void oscillator_set_waveform(Oscillator* osc, WaveformType type);

// Generate one audio sample from the oscillator
// osc: Pointer to the oscillator
// Returns: Audio sample value (-1.0 to +1.0)
float oscillator_generate_sample(Oscillator* osc);

#endif // WAVEFORM_H