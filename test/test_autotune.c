// test_autotune.c
// Test bench for auto-tune functionality

#include <stdio.h>
#include <math.h>
#include "../include/autotune.h"
#include "../include/test_utils.h"
#include "pico/stdlib.h"

// ============================================================
// AUTO-TUNE UNIT TESTS
// ============================================================

// Test 1: Initialization
bool test_autotune_init(void) {
    printf("  Testing auto-tune initialization...\n");
    
    // Initialize auto-tune
    autotune_init();
    
    // Check that note table is populated
    // C2 should be ~65.41 Hz
    TEST_ASSERT_FLOAT_EQUAL(65.41f, note_table[0], 0.1f, 
                           "C2 frequency should be 65.41 Hz");
    
    // A4 (index 49) should be exactly 440 Hz
    TEST_ASSERT_FLOAT_EQUAL(440.0f, note_table[49], 0.01f, 
                           "A4 frequency should be 440 Hz");
    
    // C7 (index 59) should be ~2093 Hz
    TEST_ASSERT_FLOAT_EQUAL(2093.0f, note_table[59], 1.0f, 
                           "C7 frequency should be ~2093 Hz");
    
    TEST_PASS("Auto-tune initialization");
}

// Test 2: Find nearest note - exact match
bool test_find_nearest_note_exact(void) {
    printf("  Testing find_nearest_note with exact frequencies...\n");
    
    autotune_init();
    
    // Test exact A4
    float result = find_nearest_note(440.0f);
    TEST_ASSERT_FLOAT_EQUAL(440.0f, result, 0.01f, 
                           "440 Hz should map to 440 Hz");
    
    // Test exact C4
    result = find_nearest_note(261.63f);
    TEST_ASSERT_FLOAT_EQUAL(261.63f, result, 0.1f, 
                           "261.63 Hz should map to C4");
    
    // Test exact G4
    result = find_nearest_note(392.0f);
    TEST_ASSERT_FLOAT_EQUAL(392.0f, result, 0.1f, 
                           "392 Hz should map to G4");
    
    TEST_PASS("Find nearest note (exact)");
}

// Test 3: Find nearest note - between notes
bool test_find_nearest_note_between(void) {
    printf("  Testing find_nearest_note with off-pitch frequencies...\n");
    
    autotune_init();
    
    // Test slightly sharp A4 (442 Hz) - should snap to 440 Hz
    float result = find_nearest_note(442.0f);
    TEST_ASSERT_FLOAT_EQUAL(440.0f, result, 0.1f, 
                           "442 Hz should snap to 440 Hz");
    
    // Test slightly flat A4 (438 Hz) - should snap to 440 Hz
    result = find_nearest_note(438.0f);
    TEST_ASSERT_FLOAT_EQUAL(440.0f, result, 0.1f, 
                           "438 Hz should snap to 440 Hz");
    
    // Test between A4 and A#4 (closer to A4)
    result = find_nearest_note(445.0f);
    TEST_ASSERT_FLOAT_EQUAL(440.0f, result, 1.0f, 
                           "445 Hz should snap to A4 (440 Hz)");
    
    // Test between A4 and A#4 (closer to A#4)
    result = find_nearest_note(460.0f);
    TEST_ASSERT_FLOAT_EQUAL(466.16f, result, 1.0f, 
                           "460 Hz should snap to A#4 (466 Hz)");
    
    TEST_PASS("Find nearest note (between)");
}

// Test 4: Auto-tune processing - full correction
bool test_autotune_process_full_correction(void) {
    printf("  Testing auto-tune processing with full correction...\n");
    
    autotune_init();
    reset_autotune();
    
    // Process several samples with slightly sharp frequency
    float input_freq = 442.0f;  // Slightly sharp A4
    float strength = 1.0f;       // 100% correction
    float glide = 0.5f;          // Moderate glide
    
    // First sample - should start moving toward 440 Hz
    float result = process_autotune(input_freq, strength, glide);
    
    // After several samples, should converge to 440 Hz
    for (int i = 0; i < 50; i++) {
        result = process_autotune(input_freq, strength, glide);
    }
    
    // Should be very close to 440 Hz
    TEST_ASSERT_FLOAT_EQUAL(440.0f, result, 1.0f, 
                           "Should converge to 440 Hz with full correction");
    
    TEST_PASS("Auto-tune processing (full correction)");
}

// Test 5: Auto-tune processing - partial correction
bool test_autotune_process_partial_correction(void) {
    printf("  Testing auto-tune processing with partial correction...\n");
    
    autotune_init();
    reset_autotune();
    
    float input_freq = 442.0f;   // Slightly sharp A4
    float strength = 0.5f;        // 50% correction
    float glide = 0.5f;
    
    // Process many samples to reach steady state
    float result;
    for (int i = 0; i < 100; i++) {
        result = process_autotune(input_freq, strength, glide);
    }
    
    // With 50% strength, should be halfway between 442 and 440
    // Expected: ~441 Hz
    TEST_ASSERT_FLOAT_EQUAL(441.0f, result, 0.5f, 
                           "50% correction should give ~441 Hz");
    
    TEST_PASS("Auto-tune processing (partial correction)");
}

// Test 6: Auto-tune processing - no correction
bool test_autotune_process_no_correction(void) {
    printf("  Testing auto-tune processing with no correction...\n");
    
    autotune_init();
    reset_autotune();
    
    float input_freq = 442.0f;
    float strength = 0.0f;  // No correction
    float glide = 0.5f;
    
    float result = process_autotune(input_freq, strength, glide);
    
    // With 0% strength, output should equal input
    TEST_ASSERT_FLOAT_EQUAL(442.0f, result, 0.1f, 
                           "0% correction should output input frequency");
    
    TEST_PASS("Auto-tune processing (no correction)");
}

// Test 7: Auto-tune glide behavior
bool test_autotune_glide(void) {
    printf("  Testing auto-tune glide behavior...\n");
    
    autotune_init();
    reset_autotune();
    
    float input_freq = 500.0f;  // Start at 500 Hz
    float strength = 1.0f;
    float glide_fast = 1.0f;    // Instant snap
    
    // Fast glide should snap immediately
    float result = process_autotune(input_freq, strength, glide_fast);
    float target = find_nearest_note(input_freq);
    
    TEST_ASSERT_FLOAT_EQUAL(target, result, 1.0f, 
                           "Fast glide should snap quickly");
    
    // Now test slow glide
    reset_autotune();
    float glide_slow = 0.01f;  // Very slow
    
    result = process_autotune(input_freq, strength, glide_slow);
    
    // Should NOT be at target yet (still gliding)
    float diff = fabsf(result - target);
    TEST_ASSERT(diff > 1.0f, "Slow glide should not reach target immediately");
    
    TEST_PASS("Auto-tune glide behavior");
}

// Test 8: Auto-tune frequency range limits
bool test_autotune_range_limits(void) {
    printf("  Testing auto-tune with out-of-range frequencies...\n");
    
    autotune_init();
    
    // Test very low frequency
    float result = find_nearest_note(10.0f);
    TEST_ASSERT(result >= 65.0f, "Should clamp to minimum frequency");
    
    // Test very high frequency
    result = find_nearest_note(5000.0f);
    TEST_ASSERT(result <= 2100.0f, "Should clamp to maximum frequency");
    
    TEST_PASS("Auto-tune range limits");
}

// Test 9: Auto-tune frequency change detection
bool test_autotune_frequency_change(void) {
    printf("  Testing auto-tune frequency change detection...\n");
    
    autotune_init();
    reset_autotune();
    
    // Process at one frequency
    process_autotune(440.0f, 1.0f, 0.5f);
    process_autotune(440.0f, 1.0f, 0.5f);
    
    // Suddenly change frequency (should detect and update target)
    float result = process_autotune(523.0f, 1.0f, 0.5f);
    
    // Should start moving toward new target (C5 ~523 Hz)
    // Won't be there immediately due to glide
    TEST_ASSERT(result > 440.0f, "Should start moving toward new frequency");
    
    TEST_PASS("Auto-tune frequency change detection");
}

// Test 10: Auto-tune stress test - rapid changes
bool test_autotune_rapid_changes(void) {
    printf("  Testing auto-tune with rapid frequency changes...\n");
    
    autotune_init();
    reset_autotune();
    
    // Rapidly change frequencies
    float frequencies[] = {440.0f, 523.0f, 392.0f, 659.0f, 294.0f};
    int num_freqs = sizeof(frequencies) / sizeof(frequencies[0]);
    
    for (int i = 0; i < num_freqs; i++) {
        for (int j = 0; j < 10; j++) {
            float result = process_autotune(frequencies[i], 1.0f, 0.3f);
            
            // Should always return a valid frequency
            TEST_ASSERT(result >= 60.0f && result <= 2100.0f, 
                       "Output should always be in valid range");
        }
    }
    
    TEST_PASS("Auto-tune rapid changes");
}

// ============================================================
// AUTO-TUNE TEST SUITE RUNNER
// ============================================================

void run_autotune_tests(int* total, int* passed, int* failed) {
    print_test_header("AUTO-TUNE TEST SUITE");
    
    int tests_passed = 0;
    int tests_failed = 0;
    int total_tests = 0;
    
    // Run all tests
    RUN_TEST(test_autotune_init);
    RUN_TEST(test_find_nearest_note_exact);
    RUN_TEST(test_find_nearest_note_between);
    RUN_TEST(test_autotune_process_full_correction);
    RUN_TEST(test_autotune_process_partial_correction);
    RUN_TEST(test_autotune_process_no_correction);
    RUN_TEST(test_autotune_glide);
    RUN_TEST(test_autotune_range_limits);
    RUN_TEST(test_autotune_frequency_change);
    RUN_TEST(test_autotune_rapid_changes);
    
    // Update totals
    *total += total_tests;
    *passed += tests_passed;
    *failed += tests_failed;
    
    printf("\nAuto-tune Suite: %d/%d tests passed\n", 
           tests_passed, total_tests);
}