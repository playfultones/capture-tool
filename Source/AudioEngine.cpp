#include "AudioEngine.h"
#include <cmath>

AudioEngine::AudioEngine() {
  // Register audio formats (WAV support)
  formatManager.registerBasicFormats();

  // NOTE: Audio device initialization is deferred to initialize()
  // This avoids triggering macOS microphone permission dialogs at app launch
  // Change listener is also deferred to avoid crashes from device change
  // notifications before the device manager is properly set up
}

bool AudioEngine::initialize() {
  if (initialized)
    return true; // Already initialized

  DBG("AudioEngine: Initializing audio devices...");

  // Initialize the audio device manager with default devices
  // Request a high channel count to support multi-channel interfaces
  // This is the point where macOS will show the microphone permission dialog
  auto result = deviceManager.initialiseWithDefaultDevices(64, 64);

  if (result.isNotEmpty()) {
    DBG("AudioEngine: Failed to initialize: " + result);
    return false;
  }

  // Enable all input and output channels on the default device
  // Done in one setAudioDeviceSetup call to minimize permission triggers
  if (auto *device = deviceManager.getCurrentAudioDevice()) {
    auto setup = deviceManager.getAudioDeviceSetup();

    int numInputChannels = device->getInputChannelNames().size();
    int numOutputChannels = device->getOutputChannelNames().size();

    setup.inputChannels.clear();
    for (int i = 0; i < numInputChannels; ++i)
      setup.inputChannels.setBit(i);

    setup.outputChannels.clear();
    for (int i = 0; i < numOutputChannels; ++i)
      setup.outputChannels.setBit(i);

    // Use treatAsChosenDevice=false to avoid re-triggering device selection
    auto setupResult = deviceManager.setAudioDeviceSetup(setup, false);
    if (setupResult.isNotEmpty())
      DBG("AudioEngine: Warning - channel setup failed: " + setupResult);
    else
      DBG("AudioEngine: Enabled " + juce::String(numInputChannels) +
          " input channels, " + juce::String(numOutputChannels) +
          " output channels");

    // Cache the channel counts for the current device
    inputChannelCountCache[setup.inputDeviceName] = numInputChannels;
    outputChannelCountCache[setup.outputDeviceName] = numOutputChannels;
  }

  // Register as the audio callback
  deviceManager.addAudioCallback(this);

  // Listen for device list changes (e.g., when new audio interface is
  // connected) Must be done AFTER initialization to avoid crashes from device
  // change notifications before the device manager has an active device
  deviceManager.addChangeListener(this);

  initialized = true;
  DBG("AudioEngine: Initialization complete");
  return true;
}

AudioEngine::~AudioEngine() {
  // Close monitor device if open
  closeMonitorDevice();

  // Stop recording if active
  stopRecording();

  if (initialized) {
    deviceManager.removeChangeListener(this);
    deviceManager.removeAudioCallback(this);
  }
}

//==============================================================================
// Device Enumeration

juce::StringArray AudioEngine::getInputDeviceNames() const {
  auto *currentType = deviceManager.getCurrentDeviceTypeObject();
  if (currentType == nullptr)
    return {};

  // Rescan to ensure we have the latest device list
  currentType->scanForDevices();

  return currentType->getDeviceNames(true); // true = input devices
}

juce::StringArray AudioEngine::getOutputDeviceNames() const {
  auto *currentType = deviceManager.getCurrentDeviceTypeObject();
  if (currentType == nullptr)
    return {};

  // Rescan to ensure we have the latest device list
  currentType->scanForDevices();

  return currentType->getDeviceNames(false); // false = output devices
}

int AudioEngine::getInputChannelCount(const juce::String &deviceName) const {
  // If requesting the currently active device, get count directly
  if (auto *currentDevice = deviceManager.getCurrentAudioDevice()) {
    auto setup = deviceManager.getAudioDeviceSetup();
    if (setup.inputDeviceName == deviceName) {
      int count = currentDevice->getInputChannelNames().size();
      inputChannelCountCache[deviceName] = count; // Update cache
      return count;
    }
  }

  // Check cache first to avoid creating temporary devices (which triggers
  // permission dialogs)
  auto it = inputChannelCountCache.find(deviceName);
  if (it != inputChannelCountCache.end())
    return it->second;

  // If not cached and not the current device, we need to estimate or create
  // device For macOS CoreAudio, most devices have at least 2 channels Return a
  // safe default rather than triggering another permission dialog
  DBG("AudioEngine: Channel count for '" + deviceName +
      "' not cached, returning default");
  return 2; // Safe default - actual count will be cached when device is
            // selected
}

int AudioEngine::getOutputChannelCount(const juce::String &deviceName) const {
  // If requesting the currently active device, get count directly
  if (auto *currentDevice = deviceManager.getCurrentAudioDevice()) {
    auto setup = deviceManager.getAudioDeviceSetup();
    if (setup.outputDeviceName == deviceName) {
      int count = currentDevice->getOutputChannelNames().size();
      outputChannelCountCache[deviceName] = count; // Update cache
      return count;
    }
  }

  // Check cache first to avoid creating temporary devices (which triggers
  // permission dialogs)
  auto it = outputChannelCountCache.find(deviceName);
  if (it != outputChannelCountCache.end())
    return it->second;

  // If not cached and not the current device, return a safe default
  // Actual count will be cached when device is selected
  DBG("AudioEngine: Channel count for '" + deviceName +
      "' not cached, returning default");
  return 2; // Safe default for output devices
}

//==============================================================================
// Device Selection

juce::String AudioEngine::getCurrentInputDevice() const {
  if (deviceManager.getCurrentAudioDevice() != nullptr) {
    auto setup = deviceManager.getAudioDeviceSetup();
    return setup.inputDeviceName;
  }
  return {};
}

juce::String AudioEngine::getCurrentOutputDevice() const {
  if (deviceManager.getCurrentAudioDevice() != nullptr) {
    auto setup = deviceManager.getAudioDeviceSetup();
    return setup.outputDeviceName;
  }
  return {};
}

int AudioEngine::getCurrentInputChannel() const { return selectedInputChannel; }

int AudioEngine::getCurrentOutputChannel() const {
  return selectedOutputChannel;
}

bool AudioEngine::setInputDevice(const juce::String &deviceName,
                                 int channelIndex) {
  // Ensure audio is initialized before changing devices
  if (!initialized) {
    if (!initialize())
      return false;
  }

  auto setup = deviceManager.getAudioDeviceSetup();

  // Check if we're just changing the channel on the same device
  bool sameDevice = (setup.inputDeviceName == deviceName);

  if (sameDevice) {
    // Just update the selected channel - no need to reconfigure device
    selectedInputChannel = channelIndex;
    DBG("AudioEngine: Input channel changed to " + juce::String(channelIndex));
    return true;
  }

  // Different device - need to reconfigure
  setup.inputDeviceName = deviceName;

  // Get the number of input channels for this device (may use cached or default
  // value)
  int numInputChannels = getInputChannelCount(deviceName);

  // Enable all input channels so we can address them by index in the callback
  setup.inputChannels.clear();
  for (int i = 0; i < numInputChannels; ++i)
    setup.inputChannels.setBit(i);

  // Also ensure all output channels remain enabled (device reconfiguration can
  // reset them)
  int numOutputChannels = getOutputChannelCount(setup.outputDeviceName);
  setup.outputChannels.clear();
  for (int i = 0; i < numOutputChannels; ++i)
    setup.outputChannels.setBit(i);

  // Remove callback before device change to avoid crashes during reconfiguration
  deviceManager.removeAudioCallback(this);
  
  auto result = deviceManager.setAudioDeviceSetup(setup, true);
  
  // Re-add callback after device change
  deviceManager.addAudioCallback(this);

  if (result.isEmpty()) {
    selectedInputChannel = channelIndex;

    // Now that the device is open, cache the actual channel counts
    if (auto *device = deviceManager.getCurrentAudioDevice()) {
      inputChannelCountCache[deviceName] =
          device->getInputChannelNames().size();
      auto currentSetup = deviceManager.getAudioDeviceSetup();
      outputChannelCountCache[currentSetup.outputDeviceName] =
          device->getOutputChannelNames().size();
    }

    DBG("AudioEngine: Input device set to " + deviceName + " channel " +
        juce::String(channelIndex) + " (enabled " +
        juce::String(numInputChannels) + " input, " +
        juce::String(numOutputChannels) + " output channels)");
    return true;
  }

  DBG("AudioEngine: Failed to set input device: " + result);
  return false;
}

bool AudioEngine::setOutputDevice(const juce::String &deviceName,
                                  int channelIndex) {
  // Ensure audio is initialized before changing devices
  if (!initialized) {
    if (!initialize())
      return false;
  }

  auto setup = deviceManager.getAudioDeviceSetup();

  // Check if we're just changing the channel on the same device
  bool sameDevice = (setup.outputDeviceName == deviceName);

  if (sameDevice) {
    // Just update the selected channel - no need to reconfigure device
    selectedOutputChannel = channelIndex;
    DBG("AudioEngine: Output channel changed to " + juce::String(channelIndex));
    return true;
  }

  // Different device - need to reconfigure
  setup.outputDeviceName = deviceName;

  // Get the number of output channels for this device (may use cached or
  // default value)
  int numOutputChannels = getOutputChannelCount(deviceName);

  // Enable all output channels so we can address them by index in the callback
  setup.outputChannels.clear();
  for (int i = 0; i < numOutputChannels; ++i)
    setup.outputChannels.setBit(i);

  // Also ensure all input channels remain enabled (device reconfiguration can
  // reset them)
  int numInputChannels = getInputChannelCount(setup.inputDeviceName);
  setup.inputChannels.clear();
  for (int i = 0; i < numInputChannels; ++i)
    setup.inputChannels.setBit(i);

  // Remove callback before device change to avoid crashes during reconfiguration
  deviceManager.removeAudioCallback(this);
  
  auto result = deviceManager.setAudioDeviceSetup(setup, true);
  
  // Re-add callback after device change
  deviceManager.addAudioCallback(this);

  if (result.isEmpty()) {
    selectedOutputChannel = channelIndex;

    // Now that the device is open, cache the actual channel counts
    if (auto *device = deviceManager.getCurrentAudioDevice()) {
      outputChannelCountCache[deviceName] =
          device->getOutputChannelNames().size();
      auto currentSetup = deviceManager.getAudioDeviceSetup();
      inputChannelCountCache[currentSetup.inputDeviceName] =
          device->getInputChannelNames().size();
    }

    DBG("AudioEngine: Output device set to " + deviceName + " channel " +
        juce::String(channelIndex) + " (enabled " +
        juce::String(numOutputChannels) + " output, " +
        juce::String(numInputChannels) + " input channels)");
    return true;
  }

  DBG("AudioEngine: Failed to set output device: " + result);
  return false;
}

//==============================================================================
// Sample Rate

juce::Array<int> AudioEngine::getAvailableSampleRates() const {
  juce::Array<int> rates;

  auto *device = deviceManager.getCurrentAudioDevice();
  if (device == nullptr)
    return rates;

  // Get available sample rates from the device
  auto availableRates = device->getAvailableSampleRates();

  // Filter to only include our supported rates: 44100, 48000, 96000
  const int supportedRates[] = {44100, 48000, 96000};

  for (int rate : supportedRates) {
    if (availableRates.contains(static_cast<double>(rate))) {
      rates.add(rate);
    }
  }

  return rates;
}

int AudioEngine::getCurrentSampleRate() const {
  auto *device = deviceManager.getCurrentAudioDevice();
  if (device == nullptr)
    return 0;

  return static_cast<int>(device->getCurrentSampleRate());
}

bool AudioEngine::setSampleRate(int sampleRate) {
  auto setup = deviceManager.getAudioDeviceSetup();
  setup.sampleRate = static_cast<double>(sampleRate);

  auto result = deviceManager.setAudioDeviceSetup(setup, true);

  if (result.isEmpty()) {
    DBG("AudioEngine: Sample rate set to " + juce::String(sampleRate) + " Hz");
    return true;
  }

  DBG("AudioEngine: Failed to set sample rate: " + result);
  return false;
}

//==============================================================================
// Level Metering

AudioEngine::MeterValues AudioEngine::getInputMeterValues() const {
  return inputMeter.getValues();
}

AudioEngine::MeterValues AudioEngine::getOutputMeterValues() const {
  return outputMeter.getValues();
}

AudioEngine::MeterValues AudioEngine::getMonitorMeterValues() const {
  return monitorMeter.getValues();
}

void AudioEngine::resetPeakHold() {
  inputMeter.resetPeakHold();
  outputMeter.resetPeakHold();
  monitorMeter.resetPeakHold();
}

//==============================================================================
// Output Gain Trim

void AudioEngine::setOutputGainTrim(float trimDb) {
  // Clamp to valid range
  trimDb = juce::jlimit(-12.0f, 12.0f, trimDb);

  // Store dB value
  outputGainTrimDb.store(trimDb, std::memory_order_relaxed);

  // Pre-compute linear gain for use in audio callback
  float linearGain = std::pow(10.0f, trimDb / 20.0f);
  outputGainLinear.store(linearGain, std::memory_order_release);

  DBG("AudioEngine: Output gain trim set to " + juce::String(trimDb, 1) +
      " dB");
}

float AudioEngine::getOutputGainTrim() const {
  return outputGainTrimDb.load(std::memory_order_relaxed);
}

//==============================================================================
// Reference Signal Loading

AudioEngine::LoadResult
AudioEngine::loadReferenceSignal(const juce::File &file) {
  LoadResult result;

  // Check if file exists
  if (!file.existsAsFile()) {
    result.errorMessage = "File does not exist";
    return result;
  }

  // Check file extension
  if (!file.hasFileExtension(".wav")) {
    result.errorMessage = "Only WAV files are supported";
    return result;
  }

  // Create a reader for the file
  std::unique_ptr<juce::AudioFormatReader> reader(
      formatManager.createReaderFor(file));

  if (reader == nullptr) {
    result.errorMessage = "Could not read audio file";
    return result;
  }

  // Validate mono - reject stereo files
  if (reader->numChannels != 1) {
    result.errorMessage = "Only mono WAV files are supported (this file has " +
                          juce::String(reader->numChannels) + " channels)";
    return result;
  }

  // Validate sample rate (44.1/48/96 kHz)
  int fileSampleRate = static_cast<int>(reader->sampleRate);
  if (fileSampleRate != 44100 && fileSampleRate != 48000 &&
      fileSampleRate != 96000) {
    result.errorMessage =
        "Unsupported sample rate: " + juce::String(fileSampleRate) +
        " Hz. Supported rates: 44.1, 48, 96 kHz";
    return result;
  }

  // Read audio data into buffer
  auto numSamples = static_cast<int>(reader->lengthInSamples);
  referenceSignalBuffer.setSize(1, numSamples);

  if (!reader->read(&referenceSignalBuffer, 0, numSamples, 0, true, false)) {
    result.errorMessage = "Failed to read audio data";
    referenceSignalBuffer.setSize(0, 0);
    return result;
  }

  // Store metadata
  referenceSignalPath = file.getFullPathName();
  referenceSignalSampleRate = fileSampleRate;

  // Fill result
  result.success = true;
  result.sampleRate = fileSampleRate;
  result.numSamples = numSamples;
  result.durationSeconds =
      static_cast<double>(numSamples) / static_cast<double>(fileSampleRate);

  DBG("AudioEngine: Loaded reference signal: " + file.getFileName() + " (" +
      juce::String(numSamples) + " samples @ " + juce::String(fileSampleRate) +
      " Hz)");

  return result;
}

bool AudioEngine::hasReferenceSignal() const {
  return referenceSignalBuffer.getNumSamples() > 0;
}

juce::String AudioEngine::getReferenceSignalPath() const {
  return referenceSignalPath;
}

int AudioEngine::getReferenceSignalSampleRate() const {
  return referenceSignalSampleRate;
}

int AudioEngine::getReferenceSignalNumSamples() const {
  return referenceSignalBuffer.getNumSamples();
}

double AudioEngine::getReferenceSignalDuration() const {
  if (referenceSignalSampleRate == 0)
    return 0.0;

  return static_cast<double>(referenceSignalBuffer.getNumSamples()) /
         static_cast<double>(referenceSignalSampleRate);
}

void AudioEngine::clearReferenceSignal() {
  // Stop playback first if active
  stopReferencePlayback();

  referenceSignalBuffer.setSize(0, 0);
  referenceSignalPath.clear();
  referenceSignalSampleRate = 0;

  DBG("AudioEngine: Reference signal cleared");
}

//==============================================================================
// Reference Signal Playback

bool AudioEngine::startReferencePlayback() {
  if (!hasReferenceSignal()) {
    DBG("AudioEngine: Cannot start playback - no reference signal loaded");
    return false;
  }

  // Reset position to start
  referencePlaybackPosition.store(0, std::memory_order_release);
  referencePlaybackActive.store(true, std::memory_order_release);

  DBG("AudioEngine: Reference playback started");
  return true;
}

void AudioEngine::stopReferencePlayback() {
  referencePlaybackActive.store(false, std::memory_order_release);
  DBG("AudioEngine: Reference playback stopped");
}

bool AudioEngine::isReferencePlaybackActive() const {
  return referencePlaybackActive.load(std::memory_order_acquire);
}

double AudioEngine::getReferencePlaybackPosition() const {
  if (!isReferencePlaybackActive() || referenceSignalSampleRate == 0)
    return 0.0;

  int position = referencePlaybackPosition.load(std::memory_order_acquire);
  return static_cast<double>(position) /
         static_cast<double>(referenceSignalSampleRate);
}

void AudioEngine::setReferencePlaybackLoop(bool enabled) {
  referencePlaybackLoop.store(enabled, std::memory_order_release);
  DBG("AudioEngine: Reference playback loop " +
      juce::String(enabled ? "enabled" : "disabled"));
}

bool AudioEngine::isReferencePlaybackLooping() const {
  return referencePlaybackLoop.load(std::memory_order_acquire);
}

//==============================================================================
// Synchronized Capture

void AudioEngine::addCaptureListener(CaptureListener *listener) {
  captureListeners.add(listener);
}

void AudioEngine::removeCaptureListener(CaptureListener *listener) {
  captureListeners.remove(listener);
}

bool AudioEngine::startCapture(const juce::File &outputFile, int tailMs) {
  // Validate preconditions
  if (!hasReferenceSignal()) {
    DBG("AudioEngine: Cannot start capture - no reference signal loaded");
    return false;
  }

  if (captureState.load(std::memory_order_acquire) != CaptureState::IDLE) {
    DBG("AudioEngine: Cannot start capture - already capturing");
    return false;
  }

  if (currentSampleRate <= 0) {
    DBG("AudioEngine: Cannot start capture - no audio device active");
    return false;
  }

  // Store capture parameters
  captureOutputFile = outputFile;
  captureTailMs = tailMs;

  // Calculate delay: 50ms of recording before playback starts
  int delaySamples = static_cast<int>((50.0 / 1000.0) * currentSampleRate);
  capturePlaybackDelayRemaining.store(delaySamples, std::memory_order_release);
  capturePlaybackStarted.store(false, std::memory_order_release);
  captureTailRemaining.store(
      -1, std::memory_order_release); // -1 = playback not finished yet

  // Start recording first
  auto recordResult = startRecording(outputFile);
  if (!recordResult.success) {
    DBG("AudioEngine: Failed to start recording: " + recordResult.errorMessage);
    return false;
  }

  // Transition to RECORDING state
  captureState.store(CaptureState::RECORDING, std::memory_order_release);
  notifyCaptureStateChanged(CaptureState::RECORDING);

  DBG("AudioEngine: Capture started - recording first, playback starts in " +
      juce::String(delaySamples) + " samples (50ms)");

  return true;
}

void AudioEngine::abortCapture() {
  if (captureState.load(std::memory_order_acquire) == CaptureState::IDLE)
    return;

  // Stop playback and recording
  stopReferencePlayback();
  stopRecording();

  // Reset state
  captureState.store(CaptureState::IDLE, std::memory_order_release);
  capturePlaybackDelayRemaining.store(0, std::memory_order_release);
  captureTailRemaining.store(-1, std::memory_order_release);
  capturePlaybackStarted.store(false, std::memory_order_release);

  notifyCaptureStateChanged(CaptureState::IDLE);

  DBG("AudioEngine: Capture aborted");
}

AudioEngine::CaptureState AudioEngine::getCaptureState() const {
  return captureState.load(std::memory_order_acquire);
}

bool AudioEngine::isCapturing() const {
  return captureState.load(std::memory_order_acquire) ==
         CaptureState::RECORDING;
}

void AudioEngine::notifyCaptureStateChanged(CaptureState state) {
  // Use MessageManager to call listeners on the message thread
  juce::MessageManager::callAsync([this, state]() {
    captureListeners.call(&CaptureListener::captureStateChanged, state);
  });
}

void AudioEngine::notifyCaptureComplete(bool success,
                                        const juce::String &errorMessage,
                                        double durationSeconds) {
  CaptureResult result;
  result.success = success;
  result.errorMessage = errorMessage;
  result.outputFilePath = captureOutputFile.getFullPathName();
  result.durationSeconds =
      (durationSeconds >= 0.0) ? durationSeconds : getRecordingDuration();

  // Use MessageManager to call listeners on the message thread
  juce::MessageManager::callAsync([this, result]() {
    captureListeners.call(&CaptureListener::captureComplete, result);
  });
}

//==============================================================================
// Monitor Output Routing

bool AudioEngine::setMonitorDevice(const juce::String &deviceName) {
  // Close existing monitor device if any
  closeMonitorDevice();

  monitorDeviceName = deviceName;

  if (deviceName.isEmpty())
    return true;

  // Open the new monitor device
  if (!openMonitorDevice(deviceName)) {
    monitorDeviceName = "";
    return false;
  }

  return true;
}

bool AudioEngine::openMonitorDevice(const juce::String &deviceName) {
  // Use a separate AudioDeviceManager for the monitor output
  // This is the proper way to handle multiple audio devices in JUCE

  // Configure the monitor device manager for output only
  juce::AudioDeviceManager::AudioDeviceSetup monitorSetup;
  monitorSetup.outputDeviceName = deviceName;
  monitorSetup.inputDeviceName = "";           // No input needed
  monitorSetup.sampleRate = currentSampleRate; // Try to match main device
  monitorSetup.bufferSize = 512;

  // Enable output channels
  monitorSetup.outputChannels.clear();
  monitorSetup.outputChannels.setBit(monitorChannelIndex);
  if (monitorStereo) {
    monitorSetup.outputChannels.setBit(monitorChannelIndex + 1);
  }

  monitorSetup.inputChannels.clear(); // No input
  monitorSetup.useDefaultInputChannels = false;
  monitorSetup.useDefaultOutputChannels = false;

  // Initialize the monitor device manager
  juce::String error =
      monitorDeviceManager.initialise(0, // numInputChannelsNeeded
                                      2, // numOutputChannelsNeeded (stereo)
                                      nullptr, // savedState
                                      true,    // selectDefaultDeviceOnFailure
                                      {},      // preferredDefaultDeviceName
                                      &monitorSetup // preferredSetupOptions
      );

  if (error.isNotEmpty())
    return false;

  // Verify the device was set correctly
  auto *monDevice = monitorDeviceManager.getCurrentAudioDevice();
  if (monDevice == nullptr)
    return false;

  monitorSampleRate = monDevice->getCurrentSampleRate();

  // Resize ring buffer if needed (enough for ~100ms at max sample rate)
  int ringBufferSize = static_cast<int>(monitorSampleRate * 0.1);
  ringBufferSize = juce::jmax(4096, ringBufferSize);
  monitorFifo.setTotalSize(ringBufferSize);
  monitorRingBuffer.setSize(1, ringBufferSize);
  monitorFifo.reset();

  // Register our callback with the monitor device manager
  monitorDeviceManager.addAudioCallback(&monitorCallback);

  return true;
}

void AudioEngine::closeMonitorDevice() {
  // Remove our callback and close the monitor device manager
  monitorDeviceManager.removeAudioCallback(&monitorCallback);
  monitorDeviceManager.closeAudioDevice();

  // Clear the ring buffer
  monitorFifo.reset();
}

juce::String AudioEngine::getMonitorDevice() const { return monitorDeviceName; }

void AudioEngine::setMonitorChannel(int channelIndex, bool stereo) {
  monitorChannelIndex = channelIndex;
  monitorStereo = stereo;

  // Re-open the monitor device if it's already open (to apply new channel
  // config)
  if (!monitorDeviceName.isEmpty()) {
    closeMonitorDevice();
    openMonitorDevice(monitorDeviceName);
  }
}

int AudioEngine::getMonitorChannel() const { return monitorChannelIndex; }

bool AudioEngine::isMonitorStereo() const { return monitorStereo; }

void AudioEngine::setMonitorSource(MonitorSource source) {
  monitorSource.store(source, std::memory_order_release);
}

AudioEngine::MonitorSource AudioEngine::getMonitorSource() const {
  return monitorSource.load(std::memory_order_acquire);
}

void AudioEngine::setMonitorGain(float gainDb) {
  // Clamp to valid range
  gainDb = juce::jlimit(-60.0f, 6.0f, gainDb);

  monitorGainDb.store(gainDb, std::memory_order_relaxed);

  // Pre-compute linear gain
  float linearGain =
      (gainDb <= -60.0f) ? 0.0f : std::pow(10.0f, gainDb / 20.0f);
  monitorGainLinear.store(linearGain, std::memory_order_release);
}

float AudioEngine::getMonitorGain() const {
  return monitorGainDb.load(std::memory_order_relaxed);
}

bool AudioEngine::isMonitorActive() const {
  auto *monDevice = monitorDeviceManager.getCurrentAudioDevice();
  return monDevice != nullptr && monDevice->isPlaying() &&
         monitorSource.load(std::memory_order_acquire) != MonitorSource::NONE;
}

//==============================================================================
// Test Tone Generator

void AudioEngine::setTestToneEnabled(bool enabled) {
  testToneEnabled.store(enabled, std::memory_order_release);
  DBG("AudioEngine: Test tone " +
      juce::String(enabled ? "enabled" : "disabled"));
}

bool AudioEngine::isTestToneEnabled() const {
  return testToneEnabled.load(std::memory_order_acquire);
}

//==============================================================================
// Recording

AudioEngine::RecordingResult
AudioEngine::startRecording(const juce::File &outputFile) {
  // Stop any existing recording first
  stopRecording();

  // Check if we have a valid sample rate
  if (currentSampleRate <= 0) {
    RecordingResult result;
    result.errorMessage = "No audio device active";
    return result;
  }

  // Use the ThreadedWavWriter module
  auto result = wavWriter.start(outputFile, currentSampleRate, 1, 16);

  if (result.success) {
    DBG("AudioEngine: Recording started to " + outputFile.getFullPathName());
  }

  return result;
}

void AudioEngine::stopRecording() {
  if (wavWriter.isRecording()) {
    wavWriter.stop();
    DBG("AudioEngine: Recording stopped");
  }
}

bool AudioEngine::isRecording() const { return wavWriter.isRecording(); }

juce::int64 AudioEngine::getRecordedSampleCount() const {
  return wavWriter.getRecordedSampleCount();
}

double AudioEngine::getRecordingDuration() const {
  return wavWriter.getDuration();
}

//==============================================================================
// AudioIODeviceCallback

void AudioEngine::audioDeviceIOCallbackWithContext(
    const float *const *inputChannelData, int numInputChannels,
    float *const *outputChannelData, int numOutputChannels, int numSamples,
    const juce::AudioIODeviceCallbackContext & /*context*/) {
  // Clear output buffer first
  for (int ch = 0; ch < numOutputChannels; ++ch) {
    if (outputChannelData[ch] != nullptr)
      juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
  }

  // Handle capture timing: start playback after delay
  const bool isInCapture =
      (captureState.load(std::memory_order_acquire) == CaptureState::RECORDING);
  if (isInCapture) {
    juce::int64 delayRemaining =
        capturePlaybackDelayRemaining.load(std::memory_order_acquire);
    if (delayRemaining > 0) {
      // Count down the delay
      delayRemaining -= numSamples;
      if (delayRemaining <= 0) {
        // Delay complete, start playback
        delayRemaining = 0;
        if (!capturePlaybackStarted.load(std::memory_order_acquire)) {
          // Reset playback position and start
          referencePlaybackPosition.store(0, std::memory_order_release);
          referencePlaybackActive.store(true, std::memory_order_release);
          capturePlaybackStarted.store(true, std::memory_order_release);
        }
      }
      capturePlaybackDelayRemaining.store(delayRemaining,
                                          std::memory_order_release);
    }
  }

  // Generate output: test tone OR reference signal playback (mutually exclusive
  // for simplicity) All output channels are enabled, so selectedOutputChannel
  // maps directly to the array index
  const bool wantTone = testToneEnabled.load(std::memory_order_acquire);
  const bool wantReferencePlayback =
      referencePlaybackActive.load(std::memory_order_acquire);

  if (numOutputChannels > 0 && selectedOutputChannel < numOutputChannels) {
    float *outputData = outputChannelData[selectedOutputChannel];
    if (outputData != nullptr) {
      if (wantReferencePlayback && referenceSignalBuffer.getNumSamples() > 0) {
        // Reference signal playback with output trim applied
        const float *refData = referenceSignalBuffer.getReadPointer(0);
        const int refNumSamples = referenceSignalBuffer.getNumSamples();
        int currentPos =
            referencePlaybackPosition.load(std::memory_order_acquire);
        const bool looping =
            referencePlaybackLoop.load(std::memory_order_acquire);
        const float outputGain =
            outputGainLinear.load(std::memory_order_acquire);
        const float targetGain = 1.0f;

        for (int i = 0; i < numSamples; ++i) {
          // Smooth fade to prevent clicks
          if (referencePlaybackFadeGain < targetGain)
            referencePlaybackFadeGain =
                std::min(referencePlaybackFadeGain + fadeIncrement, targetGain);

          if (currentPos < refNumSamples) {
            outputData[i] =
                refData[currentPos] * referencePlaybackFadeGain * outputGain;
            ++currentPos;
          } else if (looping) {
            // Loop back to start
            currentPos = 0;
            outputData[i] =
                refData[currentPos] * referencePlaybackFadeGain * outputGain;
            ++currentPos;
          } else {
            // End of file reached (no loop)
            outputData[i] = 0.0f;
          }
        }

        // Update position
        referencePlaybackPosition.store(currentPos, std::memory_order_release);

        // Stop playback when we reach the end (only if not looping)
        if (!looping && currentPos >= refNumSamples) {
          referencePlaybackActive.store(false, std::memory_order_release);
          referencePlaybackFadeGain = 0.0f;

          // If we're in a capture, start the tail countdown
          if (isInCapture &&
              capturePlaybackStarted.load(std::memory_order_acquire)) {
            int tailSamples =
                static_cast<int>((static_cast<double>(captureTailMs) / 1000.0) *
                                 currentSampleRate);
            captureTailRemaining.store(tailSamples, std::memory_order_release);
          }
        }
      } else {
        // Test tone generation with output trim applied
        const float targetGain = wantTone ? 1.0f : 0.0f;
        const float outputGain =
            outputGainLinear.load(std::memory_order_acquire);
        const double phaseIncrement =
            (2.0 * juce::MathConstants<double>::pi * testToneFrequency) /
            currentSampleRate;

        for (int i = 0; i < numSamples; ++i) {
          // Smooth fade to prevent clicks
          if (testToneFadeGain < targetGain)
            testToneFadeGain =
                std::min(testToneFadeGain + fadeIncrement, targetGain);
          else if (testToneFadeGain > targetGain)
            testToneFadeGain =
                std::max(testToneFadeGain - fadeIncrement, targetGain);

          // Generate sine wave sample with output trim applied
          float sample =
              static_cast<float>(std::sin(testTonePhase) * testToneAmplitude *
                                 testToneFadeGain * outputGain);
          outputData[i] = sample;

          // Advance phase
          testTonePhase += phaseIncrement;
          if (testTonePhase >= 2.0 * juce::MathConstants<double>::pi)
            testTonePhase -= 2.0 * juce::MathConstants<double>::pi;
        }
      }
    }
  }

  // Process input from the selected input channel
  // All input channels are enabled, so selectedInputChannel maps directly to
  // the array index Note: No input gain trim - we record the raw input signal.
  // Output trim is used instead to control the level going to the unit.
  if (numInputChannels > 0 && selectedInputChannel < numInputChannels) {
    const float *inputData = inputChannelData[selectedInputChannel];
    if (inputData != nullptr) {
      // Update input level meter
      inputMeter.process(inputData, numSamples);

      // Recording: write raw input samples to WAV file
      wavWriter.write(&inputData, numSamples);
    }
  }

  // Handle capture tail countdown and auto-completion
  if (isInCapture) {
    juce::int64 tailRemaining =
        captureTailRemaining.load(std::memory_order_acquire);

    // Only count down if playback has finished (tailRemaining >= 0 means
    // playback finished, -1 means not yet)
    if (tailRemaining >= 0) {
      tailRemaining -= numSamples;
      if (tailRemaining <= 0) {
        // Tail complete - capture is done
        tailRemaining = 0;

        // Stop recording (will be done on message thread to avoid issues)
        // We can't call stopRecording() directly from audio thread, so signal
        // completion
        captureState.store(CaptureState::DONE, std::memory_order_release);

        // Capture the sample count now, before stopping (for duration
        // calculation)
        auto finalSampleCount = wavWriter.getRecordedSampleCount();
        auto sampleRate = currentSampleRate;

        // Schedule the cleanup on the message thread
        juce::MessageManager::callAsync([this, finalSampleCount, sampleRate]() {
          stopRecording();
          notifyCaptureStateChanged(CaptureState::DONE);

          // Calculate duration from captured values (before stopRecording
          // cleared them)
          double durationSeconds =
              (sampleRate > 0)
                  ? static_cast<double>(finalSampleCount) / sampleRate
                  : 0.0;
          notifyCaptureComplete(true, juce::String(), durationSeconds);

          // Reset to IDLE for next capture
          captureState.store(CaptureState::IDLE, std::memory_order_release);
          capturePlaybackDelayRemaining.store(0, std::memory_order_release);
          captureTailRemaining.store(-1, std::memory_order_release);
          capturePlaybackStarted.store(false, std::memory_order_release);
        });
      }
      captureTailRemaining.store(tailRemaining, std::memory_order_release);
    }
  }

  // Calculate output metering from the selected output channel
  // All output channels are enabled, so selectedOutputChannel maps directly to
  // the array index
  if (numOutputChannels > 0 && selectedOutputChannel < numOutputChannels) {
    const float *outputData = outputChannelData[selectedOutputChannel];
    if (outputData != nullptr) {
      outputMeter.process(outputData, numSamples);
    }
  }

  // Monitor output routing
  // Route selected source (input or output) to the monitor device via ring
  // buffer
  const MonitorSource currentMonitorSource =
      monitorSource.load(std::memory_order_acquire);
  auto *monDevice = monitorDeviceManager.getCurrentAudioDevice();
  const bool monitorDeviceReady =
      (monDevice != nullptr && monDevice->isPlaying());

  if (currentMonitorSource != MonitorSource::NONE && monitorDeviceReady) {
    // Determine the source buffer to monitor
    const float *monitorSourceData = nullptr;

    if (currentMonitorSource == MonitorSource::INPUT) {
      // Monitor the input signal
      if (numInputChannels > 0 && selectedInputChannel < numInputChannels) {
        monitorSourceData = inputChannelData[selectedInputChannel];
      }
    } else if (currentMonitorSource == MonitorSource::OUTPUT) {
      // Monitor the output signal
      if (numOutputChannels > 0 && selectedOutputChannel < numOutputChannels) {
        monitorSourceData = outputChannelData[selectedOutputChannel];
      }
    }

    if (monitorSourceData != nullptr) {
      const float targetGain =
          monitorGainLinear.load(std::memory_order_acquire);

      // Prepare samples with fade and gain applied
      // We'll write to a temporary buffer then push to ring buffer
      float tempBuffer[2048]; // Should be enough for any reasonable buffer size
      const int samplesToProcess = juce::jmin(numSamples, 2048);

      for (int i = 0; i < samplesToProcess; ++i) {
        // Smooth fade
        if (monitorFadeGain < 1.0f)
          monitorFadeGain = std::min(monitorFadeGain + fadeIncrement, 1.0f);

        const float sample =
            monitorSourceData[i] * monitorFadeGain * targetGain;
        tempBuffer[i] = sample;
      }

      // Update monitor metering from processed buffer
      monitorMeter.process(tempBuffer, samplesToProcess);

      // Write to ring buffer for the monitor device to read
      const int freeSpace = monitorFifo.getFreeSpace();
      const int toWrite = juce::jmin(freeSpace, samplesToProcess);

      if (toWrite > 0) {
        int start1, size1, start2, size2;
        monitorFifo.prepareToWrite(toWrite, start1, size1, start2, size2);

        float *ringData = monitorRingBuffer.getWritePointer(0);

        // First segment
        if (size1 > 0) {
          juce::FloatVectorOperations::copy(ringData + start1, tempBuffer,
                                            size1);
        }

        // Second segment (wrap-around)
        if (size2 > 0) {
          juce::FloatVectorOperations::copy(ringData + start2,
                                            tempBuffer + size1, size2);
        }

        monitorFifo.finishedWrite(toWrite);
      }
    }
  } else {
    // Reset fade and metering when monitoring is disabled
    monitorFadeGain = 0.0f;
    monitorMeter.reset();
  }
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice *device) {
  if (device != nullptr) {
    currentSampleRate = device->getCurrentSampleRate();
    DBG("AudioEngine: Device started at " + juce::String(currentSampleRate) +
        " Hz");

    // Debug: Log active channels
    auto activeInputs = device->getActiveInputChannels();
    auto activeOutputs = device->getActiveOutputChannels();
    DBG("AudioEngine: Active input channels: " + activeInputs.toString(2));
    DBG("AudioEngine: Active output channels: " + activeOutputs.toString(2));
    DBG("AudioEngine: Input channel names: " +
        device->getInputChannelNames().joinIntoString(", "));
    DBG("AudioEngine: Output channel names: " +
        device->getOutputChannelNames().joinIntoString(", "));

    // Update meter sample rates for correct RMS integration time
    inputMeter.setSampleRate(currentSampleRate);
    outputMeter.setSampleRate(currentSampleRate);
  }

  // Reset test tone state
  testTonePhase = 0.0;
  testToneFadeGain = 0.0f;
}

void AudioEngine::audioDeviceStopped() { DBG("AudioEngine: Device stopped"); }

//==============================================================================
// Listener management

void AudioEngine::addListener(Listener *listener) { listeners.add(listener); }

void AudioEngine::removeListener(Listener *listener) {
  listeners.remove(listener);
}

//==============================================================================
// ChangeListener callback

void AudioEngine::changeListenerCallback(juce::ChangeBroadcaster *source) {
  // AudioDeviceManager broadcasts a change when the device list changes
  if (source == &deviceManager) {
    DBG("AudioEngine: Audio device change detected, rescanning...");

    // Force a rescan to update the cached device list
    if (auto *deviceType = deviceManager.getCurrentDeviceTypeObject()) {
      deviceType->scanForDevices();
    }

    DBG("AudioEngine: Notifying listeners of device list change");
    listeners.call(&Listener::audioDeviceListChanged);
  }
}

//==============================================================================
// Monitor Device Callback

void AudioEngine::MonitorDeviceCallback::audioDeviceIOCallbackWithContext(
    const float *const * /*inputChannelData*/, int /*numInputChannels*/,
    float *const *outputChannelData, int numOutputChannels, int numSamples,
    const juce::AudioIODeviceCallbackContext & /*context*/) {
  // Clear output first
  for (int ch = 0; ch < numOutputChannels; ++ch) {
    if (outputChannelData[ch] != nullptr) {
      juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
    }
  }

  // Check if we have data in the ring buffer
  const int available = owner.monitorFifo.getNumReady();
  const int toRead = juce::jmin(available, numSamples);

  if (toRead > 0) {
    int start1, size1, start2, size2;
    owner.monitorFifo.prepareToRead(toRead, start1, size1, start2, size2);

    const float *ringData = owner.monitorRingBuffer.getReadPointer(0);

    // The callback receives channels sequentially (0, 1, ...) for whichever
    // channels we enabled when opening the device.
    // Since we open with only the monitor channels enabled,
    // outputChannelData[0] is the first enabled channel, etc.
    // So we should write to channels 0 and 1 (if stereo) regardless of
    // what monitorChannelIndex is set to.
    const int ch1 = 0; // First enabled output channel
    const int ch2 = owner.monitorStereo
                        ? 1
                        : -1; // Second enabled output channel (if stereo)

    // Copy from ring buffer to output (mono signal to one or two channels)
    int destPos = 0;

    // First segment
    if (size1 > 0) {
      if (ch1 >= 0 && ch1 < numOutputChannels &&
          outputChannelData[ch1] != nullptr) {
        juce::FloatVectorOperations::copy(outputChannelData[ch1] + destPos,
                                          ringData + start1, size1);
      }
      if (ch2 >= 0 && ch2 < numOutputChannels &&
          outputChannelData[ch2] != nullptr) {
        juce::FloatVectorOperations::copy(outputChannelData[ch2] + destPos,
                                          ringData + start1, size1);
      }
      destPos += size1;
    }

    // Second segment (wrap-around)
    if (size2 > 0) {
      if (ch1 >= 0 && ch1 < numOutputChannels &&
          outputChannelData[ch1] != nullptr) {
        juce::FloatVectorOperations::copy(outputChannelData[ch1] + destPos,
                                          ringData + start2, size2);
      }
      if (ch2 >= 0 && ch2 < numOutputChannels &&
          outputChannelData[ch2] != nullptr) {
        juce::FloatVectorOperations::copy(outputChannelData[ch2] + destPos,
                                          ringData + start2, size2);
      }
    }

    owner.monitorFifo.finishedRead(toRead);
  }

  // If we didn't have enough data, the rest stays silent (already cleared)
}

void AudioEngine::MonitorDeviceCallback::audioDeviceAboutToStart(
    juce::AudioIODevice *device) {
  if (device != nullptr) {
    owner.monitorSampleRate = device->getCurrentSampleRate();
    DBG("AudioEngine: Monitor device started at " +
        juce::String(owner.monitorSampleRate) + " Hz");

    // Update monitor meter sample rate for correct RMS integration time
    owner.monitorMeter.setSampleRate(owner.monitorSampleRate);
  }
}

void AudioEngine::MonitorDeviceCallback::audioDeviceStopped() {
  DBG("AudioEngine: Monitor device stopped");
}
