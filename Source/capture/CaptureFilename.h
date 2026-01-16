#pragma once

#include <juce_core/juce_core.h>
#include "CaptureList.h"
#include "CaptureControl.h"

/**
 * Generates standardized filenames for captured audio files.
 * 
 * Filename format: {signal}_{samplerate}_{control1}-{value1}_{control2}-{value2}.wav
 * 
 * Example: "chirp_48k_Gain-5_Tone-7.wav"
 */
class CaptureFilenameGenerator
{
public:
    CaptureFilenameGenerator() = default;

    /**
     * Format sample rate for filename.
     * 
     * @param sampleRate The sample rate in Hz
     * @return Compact format: 44100 -> "44k1", 48000 -> "48k", 96000 -> "96k"
     */
    static juce::String formatSampleRateForFilename(int sampleRate);

    /**
     * Generate standardized filename for a capture.
     * 
     * @param item The capture item with control values
     * @param controls The capture controls (for ordering)
     * @param referenceSignalPath Path to reference signal (for base name)
     * @param sampleRate Current sample rate
     * @return Generated filename with .wav extension
     */
    static juce::String generateFilename(const CaptureItem& item,
                                         const juce::Array<CaptureControl>& controls,
                                         const juce::String& referenceSignalPath,
                                         int sampleRate);

    /**
     * Get the expected output file path for a capture item.
     * 
     * @param item The capture item
     * @param controls The capture controls
     * @param referenceSignalPath Path to reference signal
     * @param sampleRate Current sample rate
     * @param outputFolder The output folder
     * @return Full path to expected output file, or empty if output folder invalid
     */
    static juce::String getExpectedOutputPath(const CaptureItem& item,
                                              const juce::Array<CaptureControl>& controls,
                                              const juce::String& referenceSignalPath,
                                              int sampleRate,
                                              const juce::File& outputFolder);

private:
    /**
     * Sanitize a string for use in filenames.
     * Removes characters that are problematic in file paths.
     */
    static juce::String sanitizeForFilename(const juce::String& str);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CaptureFilenameGenerator)
};
