// test_utils.h
// Utilities for testing theremin components

#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// ============================================================
// TEST FRAMEWORK MACROS
// ============================================================

// Color codes for terminal output
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"

// Test assertion macros
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf(COLOR_RED "✗ FAIL: %s" COLOR_RESET "\n", message); \
            printf("  Line %d in %s\n", __LINE__, __FILE__); \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL(expected, actual, message) \
    do { \
        if ((expected) != (actual)) { \
            printf(COLOR_RED "✗ FAIL: %s" COLOR_RESET "\n", message); \
            printf("  Expected: %d, Got: %d\n", (int)(expected), (int)(actual)); \
            printf("  Line %d in %s\n", __LINE__, __FILE__); \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_FLOAT_EQUAL(expected, actual, tolerance, message) \
    do { \
        float diff = fabsf((expected) - (actual)); \
        if (diff > (tolerance)) { \
            printf(COLOR_RED "✗ FAIL: %s" COLOR_RESET "\n", message); \
            printf("  Expected: %.4f, Got: %.4f (diff: %.4f)\n", \
                   (expected), (actual), diff); \
            printf("  Line %d in %s\n", __LINE__, __FILE__); \
            return false; \
        } \
    } while(0)

#define TEST_PASS(message) \
    do { \
        printf(COLOR_GREEN "✓ PASS: %s" COLOR_RESET "\n", message); \
        return true; \
    } while(0)

// Test runner macros
#define RUN_TEST(test_func) \
    do { \
        printf(COLOR_CYAN "\nRunning: %s" COLOR_RESET "\n", #test_func); \
        if (test_func()) { \
            tests_passed++; \
        } else { \
            tests_failed++; \
        } \
        total_tests++; \
    } while(0)

// ============================================================
// TEST STATISTICS
// ============================================================

typedef struct {
    int total;
    int passed;
    int failed;
} TestStats;

// Global test statistics
extern TestStats test_stats;

// ============================================================
// TEST HELPER FUNCTIONS
// ============================================================

// Print test header
void print_test_header(const char* test_suite_name);

// Print test summary
void print_test_summary(int total, int passed, int failed);

// Check if two floats are approximately equal
bool floats_equal(float a, float b, float tolerance);

// Generate test frequency sweep
void generate_frequency_sweep(float* buffer, int size, 
                             float start_freq, float end_freq);

// Calculate RMS (Root Mean Square) of buffer
float calculate_rms(float* buffer, int size);

// Check if waveform has expected properties
bool validate_waveform_properties(float* buffer, int size, 
                                 float expected_freq, float sample_rate);

#endif // TEST_UTILS_H