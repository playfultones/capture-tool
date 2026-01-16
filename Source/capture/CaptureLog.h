#pragma once

#include <juce_core/juce_core.h>
#include "CaptureList.h"

/**
 * Manages the capture log JSON file in the output folder.
 * 
 * The capture log records metadata about each capture session
 * and individual captures for use by the ML pipeline.
 * 
 * Log format:
 * {
 *     "session": {
 *         "created": "2024-01-15T10:30:00Z",
 *         "referenceSignal": "chirp.wav",
 *         "sampleRate": 48000,
 *         "tailMs": 500
 *     },
 *     "captures": [
 *         {
 *             "id": "...",
 *             "timestamp": "2024-01-15T10:31:00Z",
 *             "settings": { "Gain": "5", "Tone": "7" },
 *             "outputFile": "chirp_48k_Gain-5_Tone-7.wav",
 *             "durationMs": 5500,
 *             "inputLevelPeak": -6.0,
 *             "inputLevelRms": -12.0
 *         }
 *     ]
 * }
 */
class CaptureLogManager
{
public:
    CaptureLogManager() = default;

    /**
     * Set the output folder for the capture log.
     * Resets the initialization state.
     * 
     * @param folder The output folder
     */
    void setOutputFolder(const juce::File& folder);

    /**
     * Get the output folder.
     */
    const juce::File& getOutputFolder() const { return outputFolder; }

    /**
     * Check if the log has been initialized for the current session.
     */
    bool isInitialized() const { return initialized; }

    /**
     * Reset the log state (mark as uninitialized).
     * Call this when starting a new session.
     */
    void reset();

    /**
     * Initialize the capture log with session metadata.
     * Called automatically on first capture if not already initialized.
     * 
     * @param referenceSignalPath Path to the reference signal file
     * @param sampleRate Current sample rate
     * @param tailMs Recording tail duration in ms
     */
    void initializeLog(const juce::String& referenceSignalPath,
                       int sampleRate,
                       int tailMs);

    /**
     * Append a capture entry to the log.
     * Automatically initializes the log if not already done.
     * 
     * @param item The capture item
     * @param referenceSignalPath Path to reference signal (for init if needed)
     * @param sampleRate Sample rate (for init if needed)
     * @param tailMs Tail duration (for init if needed)
     * @param durationSeconds Duration of the capture in seconds
     * @param peakLevelDb Peak input level in dB
     * @param rmsLevelDb RMS input level in dB
     */
    void appendCapture(const CaptureItem& item,
                       const juce::String& referenceSignalPath,
                       int sampleRate,
                       int tailMs,
                       double durationSeconds,
                       float peakLevelDb,
                       float rmsLevelDb);

    /**
     * Get current ISO 8601 timestamp.
     */
    static juce::String getCurrentTimestamp();

private:
    juce::File outputFolder;
    juce::File logFile;
    bool initialized = false;

    /**
     * Read existing capture log or create empty structure.
     */
    juce::var readOrCreateLog() const;

    /**
     * Write capture log data to file.
     * 
     * @param logData The log data to write
     * @return true if successful
     */
    bool writeLog(const juce::var& logData);

    /**
     * Create an empty log structure with session info.
     */
    juce::var createEmptyLogStructure() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CaptureLogManager)
};
