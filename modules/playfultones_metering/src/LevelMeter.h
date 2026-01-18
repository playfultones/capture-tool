#pragma once

#include <atomic>
#include <cmath>

namespace playfultones
{

/**
 * Thread-safe metering values structure.
 * Values are in dBFS (decibels relative to full scale).
 */
struct MeterValues
{
    float rmsDb = -100.0f;      // Current RMS level in dBFS
    float peakDb = -100.0f;     // Current peak level in dBFS
    float rmsHoldDb = -100.0f;  // Maximum RMS level since last reset (in dBFS)
    float peakHoldDb = -100.0f; // Maximum peak level since last reset (in dBFS)
};

/**
 * Thread-safe audio level meter.
 * 
 * Call process() from the audio thread to update levels,
 * then call getValues() from any thread to read current levels.
 * 
 * Uses atomic operations for lock-free thread safety.
 */
class LevelMeter
{
public:
    LevelMeter() = default;
    ~LevelMeter() = default;

    /**
     * Set the sample rate for proper RMS integration time.
     * Call this when the sample rate changes.
     * 
     * @param sampleRate Current sample rate in Hz
     */
    void setSampleRate(double sampleRate);

    /**
     * Process a block of audio samples and update the meter.
     * Call this from the audio callback.
     * 
     * @param data Pointer to audio samples
     * @param numSamples Number of samples to process
     */
    void process(const float* data, int numSamples);

    /**
     * Get the current meter values in dBFS.
     * Thread-safe, can be called from any thread.
     * 
     * @return Current RMS and peak levels
     */
    MeterValues getValues() const;

    /**
     * Reset the meter to minimum levels.
     */
    void reset();

    /**
     * Reset only the peak hold value to minimum.
     * Use this to clear the peak hold display without affecting current levels.
     */
    void resetPeakHold();

    //==========================================================================
    // Static utility functions

    /**
     * Convert linear amplitude to dBFS.
     * 
     * @param linear Linear amplitude (0.0 to 1.0+)
     * @return Level in dBFS (-100 for silence)
     */
    static float linearToDb(float linear);

    /**
     * Convert dBFS to linear amplitude.
     * 
     * @param db Level in dBFS
     * @return Linear amplitude
     */
    static float dbToLinear(float db);

    /**
     * Calculate RMS and peak values for a buffer of samples.
     * 
     * @param data Pointer to audio samples
     * @param numSamples Number of samples
     * @param outRms Output: RMS value (linear)
     * @param outPeak Output: Peak value (linear)
     */
    static void calculateLevels(const float* data, int numSamples, 
                                float& outRms, float& outPeak);

private:
    std::atomic<float> rms{0.0f};
    std::atomic<float> peak{0.0f};
    std::atomic<float> rmsHold{0.0f};
    std::atomic<float> peakHold{0.0f};
    
    // RMS integration using exponential moving average
    // Integration time ~300ms (AES-17 standard)
    static constexpr double kDefaultIntegrationTimeMs = 300.0;
    double rmsCoefficient{0.9997};  // Will be recalculated based on sample rate
    double rmsSquaredAccum{0.0};    // Running accumulator for squared samples (not atomic - only used in audio thread)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};

} // namespace playfultones
