#pragma once

#include <atomic>
#include <memory>

namespace playfultones
{

/**
 * Result of a recording start operation.
 */
struct RecordingResult
{
    bool success = false;
    juce::String errorMessage;
};

/**
 * Thread-safe WAV file recorder using JUCE's threaded writer.
 * 
 * This class encapsulates the complexity of setting up a threaded
 * audio file writer that can be fed from an audio callback without
 * blocking.
 * 
 * Usage:
 *   1. Call start() to begin recording to a file
 *   2. Call write() from your audio callback
 *   3. Call stop() when done (or let destructor handle it)
 */
class ThreadedWavWriter
{
public:
    ThreadedWavWriter();
    ~ThreadedWavWriter();

    /**
     * Start recording to a WAV file.
     * 
     * @param outputFile The file to write to (will be overwritten)
     * @param sampleRate Sample rate in Hz
     * @param numChannels Number of audio channels (default: 1 for mono)
     * @param bitsPerSample Bit depth (default: 16)
     * @return RecordingResult with success status or error message
     */
    RecordingResult start(const juce::File& outputFile,
                          double sampleRate,
                          int numChannels = 1,
                          int bitsPerSample = 16);

    /**
     * Stop recording and finalize the WAV file.
     * Safe to call even if not recording.
     * Will flush any remaining data to disk.
     */
    void stop();

    /**
     * Check if recording is currently active.
     * Thread-safe.
     * 
     * @return true if recording
     */
    bool isRecording() const;

    /**
     * Write audio samples to the file.
     * Call this from your audio callback.
     * Thread-safe and non-blocking.
     * 
     * @param channelData Array of pointers to channel data
     * @param numSamples Number of samples to write
     */
    void write(const float* const* channelData, int numSamples);

    /**
     * Get the number of samples written so far.
     * Thread-safe.
     * 
     * @return Number of samples written
     */
    juce::int64 getRecordedSampleCount() const;

    /**
     * Get the current recording duration in seconds.
     * 
     * @return Duration in seconds (0 if not recording)
     */
    double getDuration() const;

    /**
     * Get the output file path.
     * 
     * @return Full path to output file, or empty if not recording
     */
    juce::String getOutputFilePath() const;

private:
    juce::TimeSliceThread recordingThread{"WAV Recording Thread"};
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    juce::CriticalSection writerLock;
    
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter{nullptr};
    std::atomic<juce::int64> recordedSampleCount{0};
    
    double currentSampleRate = 0.0;
    juce::File currentOutputFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThreadedWavWriter)
};

} // namespace playfultones
