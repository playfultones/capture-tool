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
    float rmsDb = -100.0f;   // RMS level in dBFS
    float peakDb = -100.0f;  // Peak level in dBFS
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};

} // namespace playfultones
