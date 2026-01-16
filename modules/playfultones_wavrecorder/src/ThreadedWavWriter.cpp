#include "ThreadedWavWriter.h"

namespace playfultones
{

ThreadedWavWriter::ThreadedWavWriter()
{
    recordingThread.startThread();
}

ThreadedWavWriter::~ThreadedWavWriter()
{
    stop();
    recordingThread.stopThread(1000);
}

RecordingResult ThreadedWavWriter::start(const juce::File& outputFile,
                                          double sampleRate,
                                          int numChannels,
                                          int bitsPerSample)
{
    RecordingResult result;

    // Stop any existing recording first
    stop();

    // Validate parameters
    if (sampleRate <= 0)
    {
        result.errorMessage = "Invalid sample rate";
        return result;
    }

    if (numChannels < 1)
    {
        result.errorMessage = "Invalid channel count";
        return result;
    }

    // Delete existing file
    outputFile.deleteFile();

    // Create output stream
    std::unique_ptr<juce::OutputStream> fileStream(outputFile.createOutputStream());
    if (fileStream == nullptr)
    {
        result.errorMessage = "Could not create output file: " + outputFile.getFullPathName();
        return result;
    }

    // Create WAV writer
    juce::WavAudioFormat wavFormat;
    
    using Opts = juce::AudioFormatWriterOptions;
    auto writer = wavFormat.createWriterFor(
        fileStream,
        Opts{}.withSampleRate(sampleRate)
              .withNumChannels(numChannels)
              .withBitsPerSample(bitsPerSample));

    if (writer == nullptr)
    {
        result.errorMessage = "Could not create WAV writer";
        return result;
    }

    // Create threaded writer (buffer size ~0.7s at 48kHz)
    threadedWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
        writer.release(), recordingThread, 32768);

    // Store state
    currentSampleRate = sampleRate;
    currentOutputFile = outputFile;
    recordedSampleCount.store(0, std::memory_order_release);

    // Activate the writer (thread-safe)
    {
        const juce::ScopedLock sl(writerLock);
        activeWriter.store(threadedWriter.get(), std::memory_order_release);
    }

    result.success = true;
    return result;
}

void ThreadedWavWriter::stop()
{
    // Clear active writer first to stop audio callback from using it
    {
        const juce::ScopedLock sl(writerLock);
        activeWriter.store(nullptr, std::memory_order_release);
    }

    // Now safe to destroy the threaded writer (flushes remaining data)
    threadedWriter.reset();
    currentSampleRate = 0.0;
}

bool ThreadedWavWriter::isRecording() const
{
    return activeWriter.load(std::memory_order_acquire) != nullptr;
}

void ThreadedWavWriter::write(const float* const* channelData, int numSamples)
{
    const juce::ScopedLock sl(writerLock);
    auto* writer = activeWriter.load(std::memory_order_acquire);
    
    if (writer != nullptr)
    {
        writer->write(channelData, numSamples);
        recordedSampleCount.fetch_add(numSamples, std::memory_order_relaxed);
    }
}

juce::int64 ThreadedWavWriter::getRecordedSampleCount() const
{
    return recordedSampleCount.load(std::memory_order_acquire);
}

double ThreadedWavWriter::getDuration() const
{
    if (!isRecording() || currentSampleRate <= 0)
        return 0.0;

    return static_cast<double>(getRecordedSampleCount()) / currentSampleRate;
}

juce::String ThreadedWavWriter::getOutputFilePath() const
{
    return currentOutputFile.getFullPathName();
}

} // namespace playfultones
