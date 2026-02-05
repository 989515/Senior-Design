// test_utils.c
// Implementation of test utilities

#include "../include/test_utils.h"
#include <math.h>
#include <stdio.h>

// Global test statistics
TestStats test_stats = {0, 0, 0};

// ============================================================
// HELPER FUNCTION IMPLEMENTATIONS
// ============================================================

void print_test_header(const char* test_suite_name) {
    printf("\n");
    printf("════════════════════════════════════════════════════════\n");
    printf("  %s\n", test_suite_name);
    printf("════════════════════════════════════════════════════════\n");
}

void print_test_summary(int total, int passed, int failed) {
    printf("\n");
    printf("════════════════════════════════════════════════════════\n");
    printf("  TEST SUMMARY\n");
    printf("════════════════════════════════════════════════════════\n");
    printf("  Total Tests:  %d\n", total);
    
    if (passed > 0) {
        printf(COLOR_GREEN "  Passed:       %d" COLOR_RESET "\n", passed);
    }
    
    if (failed > 0) {
        printf(COLOR_RED "  Failed:       %d" COLOR_RESET "\n", failed);
    }
    
    float pass_rate = (total > 0) ? (100.0f * passed / total) : 0.0f;
    
    if (pass_rate == 100.0f) {
        printf(COLOR_GREEN "  Pass Rate:    %.1f%%" COLOR_RESET "\n", pass_rate);
    } else if (pass_rate >= 70.0f) {
        printf(COLOR_YELLOW "  Pass Rate:    %.1f%%" COLOR_RESET "\n", pass_rate);
    } else {
        printf(COLOR_RED "  Pass Rate:    %.1f%%" COLOR_RESET "\n", pass_rate);
    }
    
    printf("════════════════════════════════════════════════════════\n");
    printf("\n");
}

bool floats_equal(float a, float b, float tolerance) {
    return fabsf(a - b) <= tolerance;
}

void generate_frequency_sweep(float* buffer, int size, 
                             float start_freq, float end_freq) {
    for (int i = 0; i < size; i++) {
        float t = (float)i / (float)size;
        float freq = start_freq + (end_freq - start_freq) * t;
        buffer[i] = freq;
    }
}

float calculate_rms(float* buffer, int size) {
    float sum_squares = 0.0f;
    for (int i = 0; i < size; i++) {
        sum_squares += buffer[i] * buffer[i];
    }
    return sqrtf(sum_squares / size);
}

bool validate_waveform_properties(float* buffer, int size, 
                                 float expected_freq, float sample_rate) {
    // Check 1: All values should be in range [-1.0, 1.0]
    for (int i = 0; i < size; i++) {
        if (buffer[i] < -1.0f || buffer[i] > 1.0f) {
            printf("Waveform value out of range at index %d: %.4f\n", 
                   i, buffer[i]);
            return false;
        }
    }
    
    // Check 2: Should have reasonable RMS value (not all zeros, not clipping)
    float rms = calculate_rms(buffer, size);
    if (rms < 0.1f || rms > 1.0f) {
        printf("Waveform RMS unexpected: %.4f\n", rms);
        return false;
    }
    
    return true;
}
