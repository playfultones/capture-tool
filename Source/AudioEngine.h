#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <playfultones_metering/playfultones_metering.h>
#include <playfultones_wavrecorder/playfultones_wavrecorder.h>
#include <atomic>
#include <map>

/**
 * AudioEngine handles audio device management and I/O.
 * 
 * Responsibilities:
 * - Device enumeration (input/output devices and channels)
 * - Device selection and configuration
 * - Audio stream management
 */
class AudioEngine : public juce::AudioIODeviceCallback,
                    public juce::ChangeListener
{
public:
    /** Listener interface for device list changes */
    class Listener
    {
    public:
        virtual ~Listener() = default;
        /** Called when the audio device list has changed (device connected/disconnected) */
        virtual void audioDeviceListChanged() = 0;
    };

    AudioEngine();
    ~AudioEngine() override;

    /**
     * Initialize audio device access.
     * This triggers the macOS microphone permission dialog if not already granted.
     * Call this when user explicitly selects a device or clicks "initialize audio".
     * Safe to call multiple times - only initializes once.
     * @return true if initialization succeeded
     */
    bool initialize();

    /**
     * Check if audio has been initialized.
     * @return true if initialize() has been called successfully
     */
    bool isInitialized() const { return initialized; }

    /** Add a listener for device list changes */
    void addListener(Listener* listener);

    /** Remove a listener */
    void removeListener(Listener* listener);

    //==============================================================================
    // Device Enumeration

    /** Device info structure for frontend communication */
    struct DeviceInfo
    {
        juce::String name;
        int numChannels;
    };

    /** Get list of available input device names */
    juce::StringArray getInputDeviceNames() const;

    /** Get list of available output device names */
    juce::StringArray getOutputDeviceNames() const;

    /** 
     * Get number of input channels for a specific device.
     * @param deviceName Name of the device to query
     * @return Number of input channels, or 0 if device not found
     */
    int getInputChannelCount(const juce::String& deviceName) const;

    /**
     * Get number of output channels for a specific device.
     * @param deviceName Name of the device to query
     * @return Number of output channels, or 0 if device not found
     */
    int getOutputChannelCount(const juce::String& deviceName) const;

    //==============================================================================
    // Device Selection

    /** Get currently selected input device name */
    juce::String getCurrentInputDevice() const;

    /** Get currently selected output device name */
    juce::String getCurrentOutputDevice() const;

    /** Get currently selected input channel index */
    int getCurrentInputChannel() const;

    /** Get currently selected output channel index */
    int getCurrentOutputChannel() const;

    /** 
     * Set the input device and channel.
     * @param deviceName Name of the device to use
     * @param channelIndex Channel index (0-based)
     * @return true if successful
     */
    bool setInputDevice(const juce::String& deviceName, int channelIndex);

    /**
     * Set the output device and channel.
     * @param deviceName Name of the device to use  
     * @param channelIndex Channel index (0-based)
     * @return true if successful
     */
    bool setOutputDevice(const juce::String& deviceName, int channelIndex);

    //==============================================================================
    // Sample Rate

    /**
     * Get list of available sample rates for current device.
     * @return Array of sample rates in Hz (e.g., 44100, 48000, 96000)
     */
    juce::Array<int> getAvailableSampleRates() const;

    /**
     * Get current sample rate.
     * @return Current sample rate in Hz, or 0 if no device active
     */
    int getCurrentSampleRate() const;

    /**
     * Set the sample rate.
     * @param sampleRate Desired sample rate in Hz
     * @return true if successful
     */
    bool setSampleRate(int sampleRate);

    //==============================================================================
    // Level Metering

    /** Metering values structure - using playfultones module */
    using MeterValues = playfultones::MeterValues;

    /** Get current input metering values (thread-safe) */
    MeterValues getInputMeterValues() const;

    /** Get current output metering values (thread-safe) */
    MeterValues getOutputMeterValues() const;

    /** Get current monitor output metering values (thread-safe) */
    MeterValues getMonitorMeterValues() const;

    /** Reset peak hold values on all meters */
    void resetPeakHold();

    //==============================================================================
    // Output Gain Trim

    /**
     * Set the output gain trim in dB.
     * Applied to test tone and reference signal playback.
     * Use this to reduce the signal level going to the unit when input is clipping.
     * @param trimDb Gain trim value (-12.0 to +12.0 dB)
     */
    void setOutputGainTrim(float trimDb);

    /**
     * Get the current output gain trim in dB.
     * @return Current gain trim value
     */
    float getOutputGainTrim() const;

    //==============================================================================
    // Reference Signal Loading

    /** Result of a reference signal load operation */
    struct LoadResult
    {
        bool success = false;
        juce::String errorMessage;
        
        // Metadata (valid only if success == true)
        int sampleRate = 0;
        int numSamples = 0;
        double durationSeconds = 0.0;
    };

    /**
     * Load a mono WAV file as the reference signal.
     * @param file The WAV file to load
     * @return LoadResult with success status and metadata or error message
     */
    LoadResult loadReferenceSignal(const juce::File& file);

    /** Check if a reference signal is currently loaded */
    bool hasReferenceSignal() const;

    /** Get the file path of the currently loaded reference signal */
    juce::String getReferenceSignalPath() const;

    /** Get the sample rate of the loaded reference signal (0 if none loaded) */
    int getReferenceSignalSampleRate() const;

    /** Get the number of samples in the loaded reference signal (0 if none loaded) */
    int getReferenceSignalNumSamples() const;

    /** Get the duration in seconds of the loaded reference signal (0 if none loaded) */
    double getReferenceSignalDuration() const;

    /** Clear/unload the currently loaded reference signal */
    void clearReferenceSignal();

    //==============================================================================
    // Reference Signal Playback

    /**
     * Start preview playback of the loaded reference signal.
     * Plays through the currently selected output channel.
     * @return true if playback started, false if no reference signal loaded
     */
    bool startReferencePlayback();

    /**
     * Stop preview playback of the reference signal.
     */
    void stopReferencePlayback();

    /**
     * Check if reference signal is currently playing.
     * @return true if playing
     */
    bool isReferencePlaybackActive() const;

    /**
     * Get the current playback position in seconds.
     * @return Current position, or 0 if not playing
     */
    double getReferencePlaybackPosition() const;

    /**
     * Enable or disable loop mode for reference playback.
     * When enabled, playback restarts from beginning when reaching end of file.
     * @param enabled true to enable looping
     */
    void setReferencePlaybackLoop(bool enabled);

    /**
     * Check if loop mode is enabled.
     * @return true if looping
     */
    bool isReferencePlaybackLooping() const;

    //==============================================================================
    // Recording

    /** Result of a recording start operation - using playfultones module */
    using RecordingResult = playfultones::RecordingResult;

    /**
     * Start recording input to a mono WAV file.
     * Records from the currently selected input channel at the session sample rate.
     * Uses a threaded writer to avoid audio glitches.
     * @param outputFile The WAV file to write to (will be overwritten if exists)
     * @return RecordingResult with success status or error message
     */
    RecordingResult startRecording(const juce::File& outputFile);

    /**
     * Stop recording and finalize the WAV file.
     * Safe to call even if not recording.
     */
    void stopRecording();

    /**
     * Check if currently recording.
     * @return true if recording is active
     */
    bool isRecording() const;

    /**
     * Get the number of samples recorded so far.
     * @return Number of samples recorded, or 0 if not recording
     */
    juce::int64 getRecordedSampleCount() const;

    /**
     * Get the current recording duration in seconds.
     * @return Duration in seconds, or 0 if not recording
     */
    double getRecordingDuration() const;

    //==============================================================================
    // Synchronized Capture (Playback + Recording)

    /** Capture state machine states */
    enum class CaptureState
    {
        IDLE,       // Not capturing
        RECORDING,  // Recording active (playback starts after delay)
        DONE        // Capture completed
    };

    /** Result of a capture operation */
    struct CaptureResult
    {
        bool success = false;
        juce::String errorMessage;
        juce::String outputFilePath;
        double durationSeconds = 0.0;
    };

    /** Listener interface for capture events */
    class CaptureListener
    {
    public:
        virtual ~CaptureListener() = default;
        /** Called when capture state changes */
        virtual void captureStateChanged(CaptureState newState) = 0;
        /** Called when capture completes (success or error) */
        virtual void captureComplete(const CaptureResult& result) = 0;
    };

    /** Add a capture listener */
    void addCaptureListener(CaptureListener* listener);

    /** Remove a capture listener */
    void removeCaptureListener(CaptureListener* listener);

    /**
     * Start a synchronized capture: plays reference signal while recording input.
     * Recording starts first, playback begins 50ms later.
     * Automatically stops after reference signal completes + tail duration.
     * @param outputFile The WAV file to write the capture to
     * @param tailMs Recording tail in milliseconds (additional recording after playback ends)
     * @return true if capture started, false if conditions not met (no reference signal, etc.)
     */
    bool startCapture(const juce::File& outputFile, int tailMs = 500);

    /**
     * Abort the current capture.
     * Stops both recording and playback immediately.
     */
    void abortCapture();

    /**
     * Get the current capture state.
     * @return Current state of the capture state machine
     */
    CaptureState getCaptureState() const;

    /**
     * Check if a capture is currently in progress.
     * @return true if capturing (state is RECORDING)
     */
    bool isCapturing() const;

    //==============================================================================
    // Monitor Output Routing
    
    /** Monitor source options */
    enum class MonitorSource
    {
        NONE,   // Monitoring disabled
        INPUT,  // Monitor the input signal
        OUTPUT  // Monitor the output signal
    };
    
    /**
     * Set the monitor output device.
     * @param deviceName Name of the device to use for monitoring (empty to disable)
     * @return true if successful
     */
    bool setMonitorDevice(const juce::String& deviceName);
    
    /**
     * Get the current monitor device name.
     * @return Device name, or empty string if no monitor device configured
     */
    juce::String getMonitorDevice() const;
    
    /**
     * Set the monitor output channel configuration.
     * @param channelIndex Starting channel index (0-based)
     * @param stereo If true, use channelIndex and channelIndex+1 as stereo pair
     */
    void setMonitorChannel(int channelIndex, bool stereo = true);
    
    /**
     * Get the current monitor channel index.
     * @return Channel index (0-based)
     */
    int getMonitorChannel() const;
    
    /**
     * Check if monitor is configured for stereo output.
     * @return true if stereo (mono signal sent to both channels)
     */
    bool isMonitorStereo() const;
    
    /**
     * Set the monitor source (what signal to monitor).
     * @param source NONE, INPUT, or OUTPUT
     */
    void setMonitorSource(MonitorSource source);
    
    /**
     * Get the current monitor source.
     * @return Current monitor source
     */
    MonitorSource getMonitorSource() const;
    
    /**
     * Set the monitor output gain/level in dB.
     * @param gainDb Gain in dB (-60 to +6)
     */
    void setMonitorGain(float gainDb);
    
    /**
     * Get the current monitor gain in dB.
     * @return Gain in dB
     */
    float getMonitorGain() const;
    
    /**
     * Check if monitor output is available (device configured and source set).
     * @return true if monitoring is active
     */
    bool isMonitorActive() const;

    //==============================================================================
    // Test Tone Generator

    /**
     * Enable or disable the test tone generator.
     * Generates a 1kHz sine wave at -18dBFS on the selected output channel.
     * Uses a short fade to prevent clicks.
     * @param enabled true to enable, false to disable
     */
    void setTestToneEnabled(bool enabled);

    /** Check if test tone is currently enabled */
    bool isTestToneEnabled() const;

    //==============================================================================
    // AudioIODeviceCallback

    void audioDeviceIOCallbackWithContext(
        const float* const* inputChannelData,
        int numInputChannels,
        float* const* outputChannelData,
        int numOutputChannels,
        int numSamples,
        const juce::AudioIODeviceCallbackContext& context) override;

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    //==============================================================================
    // ChangeListener (for device list changes)

    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    //==============================================================================
    // Members

    juce::AudioDeviceManager deviceManager;
    bool initialized = false;  // Track if audio devices have been initialized

    // Selected channel indices
    int selectedInputChannel = 0;
    int selectedOutputChannel = 0;
    
    // Cached device channel counts (to avoid creating temporary devices)
    mutable std::map<juce::String, int> inputChannelCountCache;
    mutable std::map<juce::String, int> outputChannelCountCache;
    
    // Helper to update channel count cache during device scan
    void updateChannelCountCache() const;

    // Level meters (using playfultones module)
    playfultones::LevelMeter inputMeter;
    playfultones::LevelMeter outputMeter;
    playfultones::LevelMeter monitorMeter;

    // Test tone generator state
    std::atomic<bool> testToneEnabled{false};
    double testTonePhase = 0.0;
    double testToneFrequency = 1000.0;  // 1kHz
    double testToneAmplitude = 0.125892541;  // -18dBFS = 10^(-18/20)
    double currentSampleRate = 44100.0;
    
    // Fade state for click-free start/stop
    float testToneFadeGain = 0.0f;
    static constexpr float fadeIncrement = 0.002f;  // ~23ms fade at 44.1kHz

    // Output gain trim (-12 to +12 dB) - applied to test tone and reference playback
    std::atomic<float> outputGainTrimDb{0.0f};
    std::atomic<float> outputGainLinear{1.0f};  // Pre-computed linear gain

    // Reference signal storage
    juce::AudioBuffer<float> referenceSignalBuffer;
    juce::String referenceSignalPath;
    int referenceSignalSampleRate = 0;

    // Reference signal playback state
    std::atomic<bool> referencePlaybackActive{false};
    std::atomic<bool> referencePlaybackLoop{false};
    std::atomic<int> referencePlaybackPosition{0};  // Current sample position
    float referencePlaybackFadeGain = 0.0f;         // For click-free start/stop

    // Audio format manager for loading files
    juce::AudioFormatManager formatManager;

    // Recording (using playfultones module)
    playfultones::ThreadedWavWriter wavWriter;

    // Device change listeners
    juce::ListenerList<Listener> listeners;

    // Capture state machine
    std::atomic<CaptureState> captureState{CaptureState::IDLE};
    juce::ListenerList<CaptureListener> captureListeners;
    juce::File captureOutputFile;
    int captureTailMs = 500;
    
    // Capture timing (in samples)
    std::atomic<juce::int64> capturePlaybackDelayRemaining{0};  // Samples until playback starts
    std::atomic<juce::int64> captureTailRemaining{-1};          // Samples of tail remaining after playback (-1 = playback not finished)
    std::atomic<bool> capturePlaybackStarted{false};            // Has playback started in this capture?
    
    // For notifying from audio thread to message thread
    void notifyCaptureStateChanged(CaptureState state);
    void notifyCaptureComplete(bool success, const juce::String& errorMessage = {}, double durationSeconds = -1.0);

    // Monitor output routing
    juce::String monitorDeviceName;           // Device name for monitor output (empty = disabled)
    int monitorChannelIndex = 0;              // Starting channel index for monitor
    bool monitorStereo = true;                // If true, output to channel and channel+1
    std::atomic<MonitorSource> monitorSource{MonitorSource::NONE};
    std::atomic<float> monitorGainDb{-6.0f};  // Monitor gain in dB (default -6dB for safety)
    std::atomic<float> monitorGainLinear{0.5f}; // Pre-computed linear gain
    float monitorFadeGain = 0.0f;             // For click-free start/stop
    
    // Separate monitor output device support
    class MonitorDeviceCallback : public juce::AudioIODeviceCallback
    {
    public:
        MonitorDeviceCallback(AudioEngine& engine) : owner(engine) {}
        
        void audioDeviceIOCallbackWithContext(
            const float* const* inputChannelData,
            int numInputChannels,
            float* const* outputChannelData,
            int numOutputChannels,
            int numSamples,
            const juce::AudioIODeviceCallbackContext& context) override;
        
        void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
        void audioDeviceStopped() override;
        
    private:
        AudioEngine& owner;
    };
    
    friend class MonitorDeviceCallback;
    MonitorDeviceCallback monitorCallback{*this};
    
    // Monitor device management
    std::unique_ptr<juce::AudioIODevice> monitorDevice;
    juce::AudioDeviceManager monitorDeviceManager;  // Separate manager for monitor
    double monitorSampleRate = 44100.0;
    
    // Ring buffer for passing audio from main callback to monitor device
    // Using AbstractFifo + audio buffer for lock-free communication
    juce::AbstractFifo monitorFifo{4096};
    juce::AudioBuffer<float> monitorRingBuffer{1, 4096};  // Mono ring buffer
    
    // Helper to open/close monitor device
    bool openMonitorDevice(const juce::String& deviceName);
    void closeMonitorDevice();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
