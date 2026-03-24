"""
Digital Theremin DSP Simulator - ENHANCED DISTINCT PROFILES
Tests all 3 sound profiles without Raspberry Pi Pico hardware

Profile 0: Auto-Tune - Clean, Robotic, Precise
Profile 1: Distortion - Aggressive, Gritty, Heavy  
Profile 2: 8-Bit - Retro, Lo-Fi, Vibrato-Heavy

Run: python theremin_simulator.py
"""

import numpy as np
import matplotlib.pyplot as plt
from scipy.io import wavfile
import math

# ============================================================
# CONFIGURATION
# ============================================================

SAMPLE_RATE = 44100
WAVETABLE_SIZE = 256
NUM_NOTES = 60
A4_FREQUENCY = 440.0
AUTOTUNE_GLIDE_RATE = 0.3
DISTORTION_AMOUNT = 0.7
DISTORTION_DRIVE_MAX = 20.0

# ============================================================
# WAVETABLE GENERATION
# ============================================================

def generate_sine_table():
    return np.sin(2 * np.pi * np.arange(WAVETABLE_SIZE) / WAVETABLE_SIZE)

def generate_square_table():
    table = np.ones(WAVETABLE_SIZE)
    table[WAVETABLE_SIZE//2:] = -1.0
    return table

def generate_sawtooth_table():
    return 2.0 * np.arange(WAVETABLE_SIZE) / WAVETABLE_SIZE - 1.0

def generate_triangle_table():
    table = np.zeros(WAVETABLE_SIZE)
    for i in range(WAVETABLE_SIZE):
        phase_pos = i / WAVETABLE_SIZE
        if phase_pos < 0.25:
            table[i] = 4.0 * phase_pos
        elif phase_pos < 0.75:
            table[i] = 2.0 - 4.0 * phase_pos
        else:
            table[i] = 4.0 * phase_pos - 4.0
    return table

# Create all wavetables
sine_table = generate_sine_table()
square_table = generate_square_table()
sawtooth_table = generate_sawtooth_table()
triangle_table = generate_triangle_table()

print("✓ Wavetables generated")

# ============================================================
# AUTO-TUNE NOTE TABLE
# ============================================================

def generate_note_table():
    """Generate 60 notes from C2 to C7 using equal temperament"""
    notes = []
    for i in range(NUM_NOTES):
        midi_note = 24 + i  # C2 is MIDI note 24
        semitones_from_a4 = midi_note - 69  # A4 is MIDI note 69
        frequency = A4_FREQUENCY * (2.0 ** (semitones_from_a4 / 12.0))
        notes.append(frequency)
    return np.array(notes)

note_table = generate_note_table()
print(f"✓ Note table generated: {note_table[0]:.2f} Hz to {note_table[-1]:.2f} Hz")

# ============================================================
# AUTO-TUNE FUNCTIONS
# ============================================================

class AutoTuneState:
    def __init__(self):
        self.current_freq = A4_FREQUENCY
        self.target_freq = A4_FREQUENCY
        self.last_input_freq = A4_FREQUENCY

autotune_state = AutoTuneState()

def find_nearest_note(input_freq):
    """Find closest note in the note table"""
    distances = np.abs(note_table - input_freq)
    nearest_idx = np.argmin(distances)
    return note_table[nearest_idx]

def process_autotune(input_freq, strength=1.0, glide_rate=0.3):
    """Apply auto-tune pitch correction"""
    # Check if frequency changed significantly
    if abs(input_freq - autotune_state.last_input_freq) > 1.0:
        autotune_state.target_freq = find_nearest_note(input_freq)
        autotune_state.last_input_freq = input_freq
    
    # Smooth glide toward target
    diff = autotune_state.target_freq - autotune_state.current_freq
    adjustment = diff * glide_rate
    autotune_state.current_freq += adjustment
    
    # Blend between input and corrected
    corrected_freq = input_freq + (autotune_state.current_freq - input_freq) * strength
    
    return corrected_freq

def reset_autotune():
    autotune_state.current_freq = A4_FREQUENCY
    autotune_state.target_freq = A4_FREQUENCY
    autotune_state.last_input_freq = A4_FREQUENCY

# ============================================================
# WAVEFORM GENERATION
# ============================================================

class WaveformGenerator:
    def __init__(self, waveform_table):
        self.waveform_table = waveform_table
        self.phase = 0
        self.phase_max = 2**32
        self.phase_increment = 0
    
    def set_frequency(self, frequency):
        """Set the oscillator frequency"""
        cycles_per_sample = frequency / SAMPLE_RATE
        self.phase_increment = int(cycles_per_sample * self.phase_max)
    
    def generate_sample(self):
        """Generate one audio sample"""
        # Get table index from top 8 bits of phase
        index = (self.phase >> 24) & 0xFF
        sample = self.waveform_table[index]
        
        # Update phase (wraps automatically with modulo)
        self.phase = (self.phase + self.phase_increment) % self.phase_max
        
        return sample

# ============================================================
# DISTORTION EFFECT
# ============================================================

def apply_distortion(input_sample, amount=0.7):
    """Apply soft clipping distortion"""
    # Calculate drive factor
    drive = 1.0 + amount * (DISTORTION_DRIVE_MAX - 1.0)
    
    # Amplify signal
    amplified = input_sample * drive
    
    # Soft clipping using tanh
    clipped = np.tanh(amplified)
    
    # Compensate for volume
    output = clipped * amount
    
    return output

# ============================================================
# ADDITIONAL EFFECTS FOR DISTINCTION
# ============================================================

def apply_tremolo(samples, rate=5.0, depth=0.5):
    """Add tremolo (amplitude modulation) effect"""
    t = np.arange(len(samples)) / SAMPLE_RATE
    modulation = 1.0 - depth * (0.5 + 0.5 * np.sin(2 * np.pi * rate * t))
    return samples * modulation

def apply_vibrato_lfo(frequency_pattern, rate=6.0, depth=15.0):
    """Add vibrato (frequency modulation)"""
    t = np.arange(len(frequency_pattern)) / SAMPLE_RATE
    vibrato = depth * np.sin(2 * np.pi * rate * t)
    return frequency_pattern + vibrato

def apply_bitcrusher(samples, bits=4):
    """Reduce bit depth for lo-fi effect"""
    # Quantize to fewer bits
    levels = 2 ** bits
    quantized = np.round(samples * levels) / levels
    return np.clip(quantized, -1.0, 1.0)

# ============================================================
# FREQUENCY PATTERNS FOR TESTING
# ============================================================

def frequency_pattern_constant(num_samples):
    """Constant 440 Hz"""
    return np.full(num_samples, 440.0)

def frequency_pattern_sweep(num_samples):
    """Linear sweep from 220 Hz to 880 Hz"""
    return np.linspace(220.0, 880.0, num_samples)

def frequency_pattern_chromatic(num_samples):
    """Chromatic scale (12 notes)"""
    notes_per_sample = num_samples // 12
    frequencies = []
    for i in range(12):
        freq = 440.0 * (2.0 ** (i / 12.0))
        frequencies.extend([freq] * notes_per_sample)
    return np.array(frequencies[:num_samples])

def frequency_pattern_melody(num_samples):
    """Simple melody pattern"""
    notes_per_sample = num_samples // 8
    # C-D-E-F-G-A-G-F melody
    melody_notes = [261.63, 293.66, 329.63, 349.23, 392.00, 440.00, 392.00, 349.23]
    frequencies = []
    for freq in melody_notes:
        frequencies.extend([freq] * notes_per_sample)
    return np.array(frequencies[:num_samples])

def frequency_pattern_off_pitch(num_samples):
    """Slightly off-pitch notes to test auto-tune"""
    notes_per_sample = num_samples // 4
    # A4 sharp, E4 flat, G4 sharp, C5 flat
    frequencies = []
    frequencies.extend([442.0] * notes_per_sample)  # A4 slightly sharp
    frequencies.extend([328.0] * notes_per_sample)  # E4 slightly flat
    frequencies.extend([395.0] * notes_per_sample)  # G4 slightly sharp
    frequencies.extend([520.0] * notes_per_sample)  # C5 slightly flat
    return np.array(frequencies[:num_samples])

# ============================================================
# ENHANCED AUDIO GENERATION - VERY DISTINCT PROFILES
# ============================================================

def generate_audio_enhanced(profile, frequency_pattern, duration=2.0):
    """
    Generate VERY DISTINCT audio for each profile
    
    Profile 0: Auto-Tune - Clean, precise, robotic
    Profile 1: Distortion - Aggressive, gritty, heavy
    Profile 2: Ambient Pad - Smooth, spacious, dreamy
    """
    num_samples = int(duration * SAMPLE_RATE)
    
    # Ensure frequency pattern matches sample count
    if len(frequency_pattern) < num_samples:
        frequency_pattern = np.tile(frequency_pattern, 
                                   (num_samples // len(frequency_pattern)) + 1)
    frequency_pattern = frequency_pattern[:num_samples]
    
    # ==================================================
    # PROFILE 0: AUTO-TUNE - Clean and Robotic
    # ==================================================
    if profile == 0:
        print("  → Auto-Tune: Clean, Precise, Robotic")
        waveform_table = sine_table
        
        # Reset auto-tune state
        reset_autotune()
        
        # Create waveform generator
        generator = WaveformGenerator(waveform_table)
        
        # Generate samples with STRONG auto-tune
        samples = np.zeros(num_samples)
        
        for i in range(num_samples):
            input_freq = frequency_pattern[i]
            
            # STRONG pitch correction - faster glide for robotic sound
            processed_freq = process_autotune(input_freq, 
                                             strength=1.0,  # 100% correction
                                             glide_rate=0.8)  # Fast glide
            
            generator.set_frequency(processed_freq)
            samples[i] = generator.generate_sample()
        
        # Add slight tremolo for "robotic" character
        samples = apply_tremolo(samples, rate=4.0, depth=0.15)
        
    # ==================================================
    # PROFILE 1: DISTORTION - Aggressive and Heavy
    # ==================================================
    elif profile == 1:
        print("  → Distortion: Aggressive, Gritty, Heavy")
        waveform_table = sawtooth_table
        
        generator = WaveformGenerator(waveform_table)
        samples = np.zeros(num_samples)
        
        # Add vibrato to input for more character
        freq_with_vibrato = apply_vibrato_lfo(frequency_pattern, 
                                              rate=5.5, depth=8.0)
        
        for i in range(num_samples):
            processed_freq = freq_with_vibrato[i]
            
            generator.set_frequency(processed_freq)
            sample = generator.generate_sample()
            
            # HEAVY distortion
            sample = apply_distortion(sample, amount=0.85)
            
            samples[i] = sample
        
        # Add extra harmonics by mixing with higher octave
        generator_octave = WaveformGenerator(square_table)
        samples_octave = np.zeros(num_samples)
        
        for i in range(num_samples):
            generator_octave.set_frequency(freq_with_vibrato[i] * 2.0)
            samples_octave[i] = generator_octave.generate_sample()
        
        # Mix in higher octave (adds brightness)
        samples = samples * 0.7 + samples_octave * 0.3
        
        # Clip hard for extra aggression
        samples = np.tanh(samples * 1.5) * 0.9
        
    # ==================================================
    # PROFILE 2: AMBIENT PAD - Smooth, Spacious, Dreamy
    # ==================================================
    else:
        print("  → Ambient Pad: Smooth, Spacious, Dreamy")
        
        # Mix multiple waveforms for richness
        generator_sine = WaveformGenerator(sine_table)
        generator_triangle = WaveformGenerator(triangle_table)
        
        samples = np.zeros(num_samples)
        
        # Add SLOW, gentle vibrato (not fast like 8-bit)
        freq_with_vibrato = apply_vibrato_lfo(frequency_pattern, 
                                              rate=2.5, depth=3.0)
        
        for i in range(num_samples):
            processed_freq = freq_with_vibrato[i]
            
            # Mix fundamental (sine) with triangle
            generator_sine.set_frequency(processed_freq)
            sample_fundamental = generator_sine.generate_sample()
            
            generator_triangle.set_frequency(processed_freq)
            sample_triangle = generator_triangle.generate_sample()
            
            # Blend waveforms
            sample = sample_fundamental * 0.6 + sample_triangle * 0.4
            
            samples[i] = sample
        
        # Add DETUNED voices for chorus effect (makes it lush/wide)
        # Voice 2: slightly sharp
        generator_detune1 = WaveformGenerator(sine_table)
        samples_detune1 = np.zeros(num_samples)
        
        for i in range(num_samples):
            freq_detune = freq_with_vibrato[i] * 1.005  # 5 cents sharp
            generator_detune1.set_frequency(freq_detune)
            samples_detune1[i] = generator_detune1.generate_sample()
        
        # Voice 3: slightly flat
        generator_detune2 = WaveformGenerator(sine_table)
        samples_detune2 = np.zeros(num_samples)
        
        for i in range(num_samples):
            freq_detune = freq_with_vibrato[i] * 0.995  # 5 cents flat
            generator_detune2.set_frequency(freq_detune)
            samples_detune2[i] = generator_detune2.generate_sample()
        
        # Mix all 3 voices (creates chorus/unison effect)
        samples = (samples * 0.5 + 
                  samples_detune1 * 0.25 + 
                  samples_detune2 * 0.25)
        
        # Add SLOW tremolo for subtle movement
        samples = apply_tremolo(samples, rate=1.5, depth=0.2)
        
        # Apply gentle low-pass filtering (remove harsh highs)
        # Simple moving average filter
        window_size = 5
        samples_filtered = np.convolve(samples, 
                                      np.ones(window_size)/window_size, 
                                      mode='same')
        
        # Reduce volume for soft, ambient character
        samples = samples_filtered * 0.7
    
    # Final clipping
    samples = np.clip(samples, -1.0, 1.0)
    
    return samples

# ============================================================
# VISUALIZATION
# ============================================================

def plot_waveform(samples, title, max_samples=1000):
    """Plot waveform"""
    plt.figure(figsize=(12, 4))
    plt.plot(samples[:max_samples])
    plt.title(title)
    plt.xlabel('Sample')
    plt.ylabel('Amplitude')
    plt.grid(True, alpha=0.3)
    plt.ylim(-1.2, 1.2)
    plt.tight_layout()

def plot_spectrum(samples, title):
    """Plot frequency spectrum"""
    # Compute FFT
    fft = np.fft.rfft(samples)
    freqs = np.fft.rfftfreq(len(samples), 1/SAMPLE_RATE)
    magnitude = np.abs(fft)
    
    plt.figure(figsize=(12, 4))
    plt.plot(freqs[:5000], magnitude[:5000])
    plt.title(f'{title} - Frequency Spectrum')
    plt.xlabel('Frequency (Hz)')
    plt.ylabel('Magnitude')
    plt.grid(True, alpha=0.3)
    plt.xlim(0, 2000)
    plt.tight_layout()

def plot_all_profiles_comparison(duration=2.0):
    """Plot all 3 profiles side-by-side for comparison"""
    freq_pattern = frequency_pattern_constant(int(SAMPLE_RATE * duration))
    
    fig, axes = plt.subplots(3, 2, figsize=(15, 10))
    fig.suptitle('Profile Comparison - Constant 440 Hz', fontsize=16)
    
    profile_names = ['Auto-Tune', 'Distortion', '8-Bit']
    
    for profile in range(3):
        audio = generate_audio_enhanced(profile, freq_pattern, duration)
        
        # Waveform plot
        axes[profile, 0].plot(audio[:1000])
        axes[profile, 0].set_title(f'{profile_names[profile]} - Waveform')
        axes[profile, 0].set_xlabel('Sample')
        axes[profile, 0].set_ylabel('Amplitude')
        axes[profile, 0].grid(True, alpha=0.3)
        axes[profile, 0].set_ylim(-1.2, 1.2)
        
        # Spectrum plot
        fft = np.fft.rfft(audio)
        freqs = np.fft.rfftfreq(len(audio), 1/SAMPLE_RATE)
        magnitude = np.abs(fft)
        
        axes[profile, 1].plot(freqs[:2000], magnitude[:2000])
        axes[profile, 1].set_title(f'{profile_names[profile]} - Spectrum')
        axes[profile, 1].set_xlabel('Frequency (Hz)')
        axes[profile, 1].set_ylabel('Magnitude')
        axes[profile, 1].grid(True, alpha=0.3)
    
    plt.tight_layout()

# ============================================================
# MAIN TEST SUITE
# ============================================================

def run_all_tests():
    """Run comprehensive test suite with ENHANCED profiles"""
    
    print("\n" + "="*70)
    print("  DIGITAL THEREMIN DSP SIMULATOR - ENHANCED DISTINCT PROFILES")
    print("="*70 + "\n")
    
    # Test 1: Constant frequency - BEST for hearing differences
    print("TEST 1: Constant 440 Hz (Best for Comparison)")
    print("-" * 70)
    freq_pattern = frequency_pattern_constant(SAMPLE_RATE * 4)
    
    for profile in range(3):
        print(f"\nProfile {profile}:")
        audio = generate_audio_enhanced(profile, freq_pattern, duration=4.0)
        
        audio_int16 = (audio * 32767).astype(np.int16)
        filename = f'profile{profile}_constant.wav'
        wavfile.write(filename, SAMPLE_RATE, audio_int16)
        print(f"  ✓ Saved: {filename}")
    
    print("\n>>> Listen to these 3 files - they should sound VERY different!")
    
    # Test 2: Chromatic scale
    print("\n" + "="*70)
    print("TEST 2: Chromatic Scale")
    print("-" * 70)
    freq_pattern = frequency_pattern_chromatic(SAMPLE_RATE * 4)
    
    for profile in range(3):
        print(f"\nProfile {profile}:")
        audio = generate_audio_enhanced(profile, freq_pattern, duration=4.0)
        
        audio_int16 = (audio * 32767).astype(np.int16)
        filename = f'profile{profile}_chromatic.wav'
        wavfile.write(filename, SAMPLE_RATE, audio_int16)
        print(f"  ✓ Saved: {filename}")
    
    # Test 3: Melody
    print("\n" + "="*70)
    print("TEST 3: Simple Melody")
    print("-" * 70)
    freq_pattern = frequency_pattern_melody(SAMPLE_RATE * 4)
    
    for profile in range(3):
        print(f"\nProfile {profile}:")
        audio = generate_audio_enhanced(profile, freq_pattern, duration=4.0)
        
        audio_int16 = (audio * 32767).astype(np.int16)
        filename = f'profile{profile}_melody.wav'
        wavfile.write(filename, SAMPLE_RATE, audio_int16)
        print(f"  ✓ Saved: {filename}")
    
    # Test 4: Frequency sweep
    print("\n" + "="*70)
    print("TEST 4: Frequency Sweep (220-880 Hz)")
    print("-" * 70)
    freq_pattern = frequency_pattern_sweep(SAMPLE_RATE * 5)
    
    for profile in range(3):
        print(f"\nProfile {profile}:")
        audio = generate_audio_enhanced(profile, freq_pattern, duration=5.0)
        
        audio_int16 = (audio * 32767).astype(np.int16)
        filename = f'profile{profile}_sweep.wav'
        wavfile.write(filename, SAMPLE_RATE, audio_int16)
        print(f"  ✓ Saved: {filename}")
    
    # Test 5: Comparison - all 3 profiles in sequence
    print("\n" + "="*70)
    print("TEST 5: All Profiles in Sequence (Comparison)")
    print("-" * 70)
    
    all_audio = []
    freq_pattern = frequency_pattern_constant(SAMPLE_RATE * 3)
    
    for profile in range(3):
        print(f"Adding Profile {profile}...")
        audio = generate_audio_enhanced(profile, freq_pattern, duration=3.0)
        all_audio.append(audio)
        
        # Add 0.5 second silence between profiles
        silence = np.zeros(SAMPLE_RATE // 2)
        all_audio.append(silence)
    
    # Concatenate all
    combined = np.concatenate(all_audio)
    audio_int16 = (combined * 32767).astype(np.int16)
    wavfile.write('comparison_all_profiles.wav', SAMPLE_RATE, audio_int16)
    print("  ✓ Saved: comparison_all_profiles.wav")
    print("  → Listen to this to hear all 3 profiles back-to-back!")
    
    # Test 6: Auto-tune demonstration
    print("\n" + "="*70)
    print("TEST 6: Auto-Tune Demonstration")
    print("-" * 70)
    freq_pattern = frequency_pattern_off_pitch(SAMPLE_RATE * 4)
    
    print("\nWith auto-tune (Profile 0):")
    audio_corrected = generate_audio_enhanced(0, freq_pattern, duration=4.0)
    audio_int16 = (audio_corrected * 32767).astype(np.int16)
    wavfile.write('autotune_corrected.wav', SAMPLE_RATE, audio_int16)
    print("  ✓ Saved: autotune_corrected.wav")
    
    print("\nWithout auto-tune (Profile 2):")
    audio_uncorrected = generate_audio_enhanced(2, freq_pattern, duration=4.0)
    audio_int16 = (audio_uncorrected * 32767).astype(np.int16)
    wavfile.write('autotune_uncorrected.wav', SAMPLE_RATE, audio_int16)
    print("  ✓ Saved: autotune_uncorrected.wav")
    print("  → Compare these to hear auto-tune in action!")
    
    # Generate comparison plots
    print("\n" + "="*70)
    print("Generating comparison plots...")
    print("-" * 70)
    plot_all_profiles_comparison(duration=1.0)
    
    # Summary
    print("\n" + "="*70)
    print("  ALL TESTS COMPLETE!")
    print("="*70)
    print("\n📁 Generated WAV Files:")
    print("   Main Tests:")
    print("   • profile0_constant.wav    - Auto-Tune (clean, robotic)")
    print("   • profile1_constant.wav    - Distortion (aggressive, gritty)")
    print("   • profile2_constant.wav    - 8-Bit (retro, vibrato)")
    print("\n   Variations:")
    print("   • profile*_chromatic.wav   - Chromatic scale")
    print("   • profile*_melody.wav      - Simple melody")
    print("   • profile*_sweep.wav       - Frequency sweep")
    print("\n   Special:")
    print("   • comparison_all_profiles.wav  - All 3 in sequence")
    print("   • autotune_corrected.wav       - With pitch correction")
    print("   • autotune_uncorrected.wav     - Without correction")
    print("\n🎧 START HERE: Listen to profile0_constant.wav, profile1_constant.wav,")
    print("               and profile2_constant.wav to hear the differences!")
    print("\n📊 Plots displayed - close windows to exit.")
    print("="*70 + "\n")
    
    plt.show()

# ============================================================
# RUN TESTS
# ============================================================

if __name__ == "__main__":
    run_all_tests()