#include "MainComponent.h"
#include <juce_audio_formats/juce_audio_formats.h>





//==============================================================================

MainComponent::MainComponent()
{
    // Create and configure the WebView first (before setSize triggers resized())
    webView = std::make_unique<juce::WebBrowserComponent>(createWebViewOptions());
    addAndMakeVisible(*webView);
    
    setSize(1024, 768);
    
    // Navigate to the resource provider root to load index.html
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    
    // Start metering timer (~30fps update rate)
    startTimerHz(30);
    
    // Listen for audio device list changes
    audioEngine.addListener(this);
    
    // Listen for capture state changes and completion events
    audioEngine.addCaptureListener(this);

#if REFCAP_E2E
    // Only active when launched by the e2e driver with --e2e-test-port=N
    if (E2EBridge::isRequested())
        e2eBridge = std::make_unique<E2EBridge>(*webView);
#endif
}

MainComponent::~MainComponent()
{
    audioEngine.removeCaptureListener(this);
    audioEngine.removeListener(this);
    stopTimer();
}

void MainComponent::resized()
{
    if (webView != nullptr)
        webView->setBounds(getLocalBounds());
}

void MainComponent::emitEvent(const juce::Identifier& eventId, const juce::var& data)
{
    if (webView != nullptr)
        webView->emitEventIfBrowserIsVisible(eventId, data);
}

juce::WebBrowserComponent::Options MainComponent::createWebViewOptions()
{
    using Options = juce::WebBrowserComponent::Options;
    using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;
    
    return Options{}
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withNativeIntegrationEnabled()
        .withResourceProvider(
            [this](const juce::String& path) { return resourceProvider(path); })
        
        // Initialize audio (triggers permission dialog if needed)
        // Call this before using audio devices
        .withNativeFunction(
            "initializeAudio",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                bool success = audioEngine.initialize();
                complete(juce::var(success));
            })
        
        // Check if audio is initialized
        .withNativeFunction(
            "isAudioInitialized",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(juce::var(audioEngine.isInitialized()));
            })
        
        // Get list of available input device names
        .withNativeFunction(
            "getInputDevices",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(ProjectSerializer::stringArrayToVar(audioEngine.getInputDeviceNames()));
            })
        
        // Get list of available output device names
        .withNativeFunction(
            "getOutputDevices",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(ProjectSerializer::stringArrayToVar(audioEngine.getOutputDeviceNames()));
            })
        
        // Get input channel count for a specific device
        // Args: [deviceName: string]
        .withNativeFunction(
            "getInputChannelCount",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                if (args.isEmpty())
                {
                    complete(juce::var(0));
                    return;
                }
                auto deviceName = args[0].toString();
                complete(juce::var(audioEngine.getInputChannelCount(deviceName)));
            })
        
        // Get output channel count for a specific device
        // Args: [deviceName: string]
        .withNativeFunction(
            "getOutputChannelCount",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                if (args.isEmpty())
                {
                    complete(juce::var(0));
                    return;
                }
                auto deviceName = args[0].toString();
                complete(juce::var(audioEngine.getOutputChannelCount(deviceName)));
            })
        
        // Get current audio state (selected devices, channels)
        .withNativeFunction(
            "getAudioState",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(getAudioStateVar());
            })
        
        // Set input device and channel
        // Args: [deviceName: string, channelIndex: number]
        .withNativeFunction(
            "setInputDevice",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                if (args.size() < 2)
                {
                    complete(juce::var("error: requires deviceName and channelIndex"));
                    return;
                }
                
                auto deviceName = args[0].toString();
                auto channelIndex = static_cast<int>(args[1]);
                
                bool success = audioEngine.setInputDevice(deviceName, channelIndex);
                if (success) markProjectDirty();
                complete(juce::var(success));
            })
        
        // Set output device and channel
        // Args: [deviceName: string, channelIndex: number]
        .withNativeFunction(
            "setOutputDevice",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                if (args.size() < 2)
                {
                    complete(juce::var("error: requires deviceName and channelIndex"));
                    return;
                }
                
                auto deviceName = args[0].toString();
                auto channelIndex = static_cast<int>(args[1]);
                
                bool success = audioEngine.setOutputDevice(deviceName, channelIndex);
                if (success) markProjectDirty();
                complete(juce::var(success));
            })
        
        // Enable/disable test tone generator
        // Args: [enabled: boolean]
        .withNativeFunction(
            "setTestToneEnabled",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                bool enabled = args.isEmpty() ? false : static_cast<bool>(args[0]);
                audioEngine.setTestToneEnabled(enabled);
                complete(juce::var(enabled));
            })
        
        // Get current test tone state
        .withNativeFunction(
            "isTestToneEnabled",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(juce::var(audioEngine.isTestToneEnabled()));
            })
        
        // Set output gain trim in dB (-12 to +12)
        // Applied to test tone and reference signal playback
        // Args: [trimDb: number]
        .withNativeFunction(
            "setOutputGainTrim",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                float trimDb = args.isEmpty() ? 0.0f : static_cast<float>(args[0]);
                audioEngine.setOutputGainTrim(trimDb);
                complete(juce::var(audioEngine.getOutputGainTrim()));
            })
        
        // Get current output gain trim in dB
        .withNativeFunction(
            "getOutputGainTrim",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(juce::var(audioEngine.getOutputGainTrim()));
            })
        
        // Reset peak hold values on all meters
        .withNativeFunction(
            "resetPeakHold",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                audioEngine.resetPeakHold();
                complete(juce::var(true));
            })
        
        // Get available sample rates for current device
        .withNativeFunction(
            "getAvailableSampleRates",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(intArrayToVar(audioEngine.getAvailableSampleRates()));
            })
        
        // Get current sample rate
        .withNativeFunction(
            "getCurrentSampleRate",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(juce::var(audioEngine.getCurrentSampleRate()));
            })
        
        // Set sample rate
        // Args: [sampleRate: number]
        .withNativeFunction(
            "setSampleRate",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                if (args.isEmpty())
                {
                    complete(juce::var("error: requires sampleRate"));
                    return;
                }
                
                auto sampleRate = static_cast<int>(args[0]);
                bool success = audioEngine.setSampleRate(sampleRate);
                if (success) markProjectDirty();
                complete(juce::var(success));
            })
        
        //==============================================================================
        // Reference Signals (Multiple per session)
        
        // Get all reference signals
        .withNativeFunction(
            "getReferenceSignals",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(getReferenceSignalsVar());
            })
        
        // Add a reference signal from file path
        // Args: [filePath: string]
        // Returns: { success, id?, errorMessage?, signal? }
        .withNativeFunction(
            "addReferenceSignal",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                if (args.isEmpty())
                {
                    auto* resultObj = new juce::DynamicObject();
                    resultObj->setProperty("success", false);
                    resultObj->setProperty("errorMessage", "File path required");
                    complete(juce::var(resultObj));
                    return;
                }
                
                juce::String filePath = args[0].toString();
                complete(addReferenceSignalFromPath(filePath));
            })
        
        // Browse and add reference signals (opens native file picker with multi-select)
        .withNativeFunction(
            "browseAndAddReferenceSignals",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                // Create file chooser for WAV files with multi-select
                fileChooser = std::make_unique<juce::FileChooser>(
                    "Select Reference Signals",
                    juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                    "*.wav"
                );
                
                // Launch async file picker with multi-select
                auto chooserFlags = juce::FileBrowserComponent::openMode 
                    | juce::FileBrowserComponent::canSelectFiles
                    | juce::FileBrowserComponent::canSelectMultipleItems;
                
                fileChooser->launchAsync(chooserFlags, [this, complete](const juce::FileChooser& fc) {
                    auto results = fc.getResults();
                    
                    if (results.isEmpty())
                    {
                        // User cancelled
                        auto* resultObj = new juce::DynamicObject();
                        resultObj->setProperty("cancelled", true);
                        complete(juce::var(resultObj));
                        return;
                    }
                    
                    // Try to add each selected file
                    juce::Array<juce::var> added;
                    juce::Array<juce::var> errors;
                    
                    for (const auto& file : results)
                    {
                        auto result = addReferenceSignalFromPath(file.getFullPathName());
                        
                        if (auto* resultObj = result.getDynamicObject())
                        {
                            if (static_cast<bool>(resultObj->getProperty("success")))
                            {
                                added.add(resultObj->getProperty("signal"));
                            }
                            else
                            {
                                auto* errorObj = new juce::DynamicObject();
                                errorObj->setProperty("fileName", file.getFileName());
                                errorObj->setProperty("errorMessage", resultObj->getProperty("errorMessage"));
                                errors.add(juce::var(errorObj));
                            }
                        }
                    }
                    
                    auto* resultObj = new juce::DynamicObject();
                    resultObj->setProperty("cancelled", false);
                    resultObj->setProperty("added", juce::var(added));
                    resultObj->setProperty("errors", juce::var(errors));
                    resultObj->setProperty("signals", getReferenceSignalsVar());
                    complete(juce::var(resultObj));
                });
            })
        
        // Remove a reference signal by ID
        // Args: [id: string]
        .withNativeFunction(
            "removeReferenceSignal",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                if (args.isEmpty())
                {
                    complete(juce::var(false));
                    return;
                }
                
                juce::String id = args[0].toString();
                
                // Find and remove the signal
                for (int i = 0; i < referenceSignals.size(); ++i)
                {
                    if (referenceSignals[i].id == id)
                    {
                        // If this was the selected preview signal, clear selection
                        if (selectedPreviewSignalId == id)
                        {
                            selectedPreviewSignalId = juce::String();
                            audioEngine.clearReferenceSignal();
                        }
                        
                        referenceSignals.remove(i);
                        markProjectDirty();
                        complete(juce::var(true));
                        return;
                    }
                }
                
                complete(juce::var(false));
            })
        
        // Set recording tail for a specific signal
        // Args: [id: string, tailMs: number]
        .withNativeFunction(
            "setReferenceSignalTail",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                if (args.size() < 2)
                {
                    complete(juce::var(-1));
                    return;
                }
                
                juce::String id = args[0].toString();
                int newTailMs = static_cast<int>(args[1]);
                
                // Validate tail value
                if (newTailMs != 0 && newTailMs != 250 && newTailMs != 500 && newTailMs != 1000)
                {
                    complete(juce::var(-1));
                    return;
                }
                
                // Find and update the signal
                if (auto* signal = findSignalById(id))
                {
                    signal->tailMs = newTailMs;
                    markProjectDirty();
                    complete(juce::var(newTailMs));
                    return;
                }
                
                complete(juce::var(-1));
            })
        
        // Select a reference signal for preview playback
        // Args: [id: string]
        .withNativeFunction(
            "selectReferenceSignalForPreview",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                if (args.isEmpty())
                {
                    complete(juce::var(false));
                    return;
                }
                
                juce::String id = args[0].toString();
                
                // Find the signal
                const auto* signal = findSignalById(id);
                if (signal == nullptr)
                {
                    complete(juce::var(false));
                    return;
                }
                
                // Load into audio engine for preview
                juce::File signalFile(signal->filePath);
                if (!signalFile.existsAsFile())
                {
                    complete(juce::var(false));
                    return;
                }
                
                auto loadResult = audioEngine.loadReferenceSignal(signalFile);
                if (loadResult.success)
                {
                    selectedPreviewSignalId = id;
                    complete(juce::var(true));
                }
                else
                {
                    complete(juce::var(false));
                }
            })
        
        // Start reference signal preview playback
        .withNativeFunction(
            "startReferencePlayback",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                bool success = audioEngine.startReferencePlayback();
                complete(juce::var(success));
            })
        
        // Stop reference signal preview playback
        .withNativeFunction(
            "stopReferencePlayback",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                audioEngine.stopReferencePlayback();
                complete(juce::var(true));
            })
        
        // Check if reference playback is active
        .withNativeFunction(
            "isReferencePlaybackActive",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(juce::var(audioEngine.isReferencePlaybackActive()));
            })
        
        // Get reference playback position in seconds
        .withNativeFunction(
            "getReferencePlaybackPosition",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(juce::var(audioEngine.getReferencePlaybackPosition()));
            })
        
        // Set reference playback loop mode
        // Args: [enabled: boolean]
        .withNativeFunction(
            "setReferencePlaybackLoop",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                bool enabled = args.isEmpty() ? false : static_cast<bool>(args[0]);
                audioEngine.setReferencePlaybackLoop(enabled);
                complete(juce::var(enabled));
            })
        
        // Check if reference playback loop is enabled
        .withNativeFunction(
            "isReferencePlaybackLooping",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(juce::var(audioEngine.isReferencePlaybackLooping()));
            })
        
        // Get calibration state
        .withNativeFunction(
            "getCalibrationState",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(getCalibrationStateVar());
            })
        
        // Set calibration state
        // Args: [state: object]
        .withNativeFunction(
            "setCalibrationState",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                if (args.isEmpty() || !args[0].isObject())
                {
                    complete(juce::var(false));
                    return;
                }
                
                calibrationState = CalibrationState::fromVar(args[0]);
                markProjectDirty();
                complete(juce::var(true));
            })
        
        //==============================================================================
        // Visual Guide State (pass-through storage for frontend)
        
        // Get guide state (returns whatever the frontend stored)
        .withNativeFunction(
            "getGuideState",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(guideState);
            })
        
        // Set guide state (stores whatever the frontend sends)
        // Args: [state: object] - { guides: Array, cameraDeviceId: string|null }
        .withNativeFunction(
            "setGuideState",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                if (args.isEmpty())
                {
                    guideState = juce::var();
                }
                else
                {
                    guideState = args[0];
                }
                markProjectDirty();
                complete(juce::var(true));
            })
        
        // Browse for output folder (opens native folder picker)
        .withNativeFunction(
            "browseOutputFolder",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                // Create folder chooser
                fileChooser = std::make_unique<juce::FileChooser>(
                    "Select Output Folder",
                    outputFolder.exists() ? outputFolder : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                );
                
                // Launch async folder picker
                auto chooserFlags = juce::FileBrowserComponent::openMode 
                    | juce::FileBrowserComponent::canSelectDirectories;
                
                fileChooser->launchAsync(chooserFlags, [this, complete](const juce::FileChooser& fc) {
                    auto results = fc.getResults();
                    
                    if (results.isEmpty())
                    {
                        // User cancelled
                        auto* resultObj = new juce::DynamicObject();
                        resultObj->setProperty("cancelled", true);
                        complete(juce::var(resultObj));
                        return;
                    }
                    
                    auto folder = results.getFirst();
                    
                    auto* resultObj = new juce::DynamicObject();
                    resultObj->setProperty("cancelled", false);
                    
                    if (folder.isDirectory())
                    {
                        // Store the selected folder
                        outputFolder = folder;
                        
                        // Reset capture log state for new folder
                        captureLogManager.setOutputFolder(folder);
                        
                        // Check if writable
                        bool isWritable = isOutputFolderWritable();
                        
                        resultObj->setProperty("success", true);
                        resultObj->setProperty("folderPath", folder.getFullPathName());
                        resultObj->setProperty("isWritable", isWritable);

                        // Persist the new output folder. Autosave only writes if a
                        // named project file already exists; it no longer fabricates
                        // a project.rcp just because an output folder is set.
                        markProjectDirty();
                    }
                    else
                    {
                        resultObj->setProperty("success", false);
                        resultObj->setProperty("errorMessage", "Selected path is not a directory");
                    }
                    
                    complete(juce::var(resultObj));
                });
            })
        
        // Get current output folder state
        .withNativeFunction(
            "getOutputFolderState",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(getOutputFolderStateVar());
            })
        
        // Reveal output folder in system file browser (Finder on macOS)
        .withNativeFunction(
            "revealOutputFolder",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                if (outputFolder.exists() && outputFolder.isDirectory())
                {
                    outputFolder.revealToUser();
                    complete(juce::var(true));
                }
                else
                {
                    complete(juce::var(false));
                }
            })
        
        // Get current capture state
        .withNativeFunction(
            "getCaptureState",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                auto* resultObj = new juce::DynamicObject();
                
                auto state = audioEngine.getCaptureState();
                juce::String stateStr;
                switch (state)
                {
                    case AudioEngine::CaptureState::IDLE: stateStr = "idle"; break;
                    case AudioEngine::CaptureState::RECORDING: stateStr = "recording"; break;
                    case AudioEngine::CaptureState::DONE: stateStr = "done"; break;
                }
                
                resultObj->setProperty("state", stateStr);
                resultObj->setProperty("isCapturing", audioEngine.isCapturing());
                
                complete(juce::var(resultObj));
            })
        
        // Start capture (synchronized playback + recording)
        // Args: [captureItemId: string, signalId: string (optional)]
        // Uses the signal's configured tail duration
        // If captureItemId is provided, uses output folder + standardized naming
        // When starting a new capture item, this resets signalIndex to 0
        // After each signal completes, the backend auto-advances to the next signal
        .withNativeFunction(
            "startCapture",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                // Parse arguments
                juce::String captureItemId = args.size() > 0 ? args[0].toString() : "";
                juce::String signalId = args.size() > 1 ? args[1].toString() : "";
                
                // Reset signal index for new capture item
                if (captureItemId.isNotEmpty() && captureItemId != currentCaptureItemId)
                {
                    currentCaptureItemId = captureItemId;
                    currentCaptureSignalIndex = 0;
                }
                
                // Find the signal to use
                const ReferenceSignal* signal = nullptr;
                if (signalId.isNotEmpty())
                {
                    signal = findSignalById(signalId);
                    // Update signal index to match
                    for (int i = 0; i < referenceSignals.size(); ++i)
                    {
                        if (referenceSignals[i].id == signalId)
                        {
                            currentCaptureSignalIndex = i;
                            break;
                        }
                    }
                }
                else if (currentCaptureSignalIndex >= 0 && currentCaptureSignalIndex < referenceSignals.size())
                {
                    // Use signal at current index
                    signal = &referenceSignals.getReference(currentCaptureSignalIndex);
                }
                else if (!referenceSignals.isEmpty())
                {
                    // Default to first signal if none specified
                    signal = &referenceSignals.getReference(0);
                    currentCaptureSignalIndex = 0;
                }
                
                if (signal == nullptr)
                {
                    auto* resultObj = new juce::DynamicObject();
                    resultObj->setProperty("success", false);
                    resultObj->setProperty("errorMessage", "No reference signal available");
                    complete(juce::var(resultObj));
                    return;
                }
                
                // Load the signal into audio engine if not already loaded
                if (audioEngine.getReferenceSignalPath() != signal->filePath)
                {
                    juce::File signalFile(signal->filePath);
                    auto loadResult = audioEngine.loadReferenceSignal(signalFile);
                    if (!loadResult.success)
                    {
                        auto* resultObj = new juce::DynamicObject();
                        resultObj->setProperty("success", false);
                        resultObj->setProperty("errorMessage", "Failed to load reference signal: " + loadResult.errorMessage);
                        complete(juce::var(resultObj));
                        return;
                    }
                }
                
                int tailMs = signal->tailMs;
                
                juce::File outputFile;
                juce::String errorMessage;
                
                // If we have a capture item ID, use standardized naming and output folder
                CaptureItem* captureItem = captureItemId.isNotEmpty() ? captureListManager.findById(captureItemId) : nullptr;
                
                if (captureItem != nullptr && outputFolder.exists() && outputFolder.isDirectory() && isOutputFolderWritable())
                {
                    // Use standardized filename in output folder
                    juce::String filename = CaptureFilenameGenerator::generateFilename(
                        *captureItem,
                        captureControlManager.getControls(),
                        audioEngine.getReferenceSignalPath(),
                        audioEngine.getCurrentSampleRate());
                    outputFile = outputFolder.getChildFile(filename);
                }
                else if (captureItem != nullptr && (!outputFolder.exists() || !isOutputFolderWritable()))
                {
                    // Output folder not configured or not writable
                    auto* resultObj = new juce::DynamicObject();
                    resultObj->setProperty("success", false);
                    resultObj->setProperty("errorMessage", "Output folder not configured or not writable. Please select an output folder.");
                    complete(juce::var(resultObj));
                    return;
                }
                else
                {
                    // Fallback: temp directory with timestamp (legacy behavior)
                    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
                    auto timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
                    outputFile = tempDir.getChildFile("capture_" + timestamp + ".wav");
                }
                
                // Start capture
                bool success = audioEngine.startCapture(outputFile, tailMs);
                
                auto* resultObj = new juce::DynamicObject();
                resultObj->setProperty("success", success);
                
                if (!success)
                {
                    resultObj->setProperty("errorMessage", "Failed to start capture");
                }
                else
                {
                    resultObj->setProperty("outputFilePath", outputFile.getFullPathName());
                    resultObj->setProperty("captureItemId", captureItemId);
                    
                    // Clear output paths if this is the first signal (new capture)
                    if (captureItem != nullptr && currentCaptureSignalIndex == 0)
                    {
                        captureItem->outputFilePaths.clear();
                    }
                    
                    // Calculate total duration for progress display
                    double refDuration = audioEngine.getReferenceSignalDuration();
                    double totalDurationMs = (refDuration * 1000.0) + 50.0 + static_cast<double>(tailMs);
                    resultObj->setProperty("totalDurationMs", totalDurationMs);
                }
                
                complete(juce::var(resultObj));
            })
        
        // Abort current capture
        .withNativeFunction(
            "abortCapture",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                audioEngine.abortCapture();
                
                // Reset multi-signal capture state
                currentCaptureItemId = juce::String();
                currentCaptureSignalIndex = 0;
                
                complete(juce::var(true));
            })
        
        //==============================================================================
        // Capture Controls (Matrix Definition)
        
        // Get all capture controls
        .withNativeFunction(
            "getCaptureControls",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(captureControlManager.toVar());
            })
        
        // Add a new capture control
        // Args: [name: string, type: string ("discrete" or "continuous"), valuesInput: string]
        .withNativeFunction(
            "addCaptureControl",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                juce::String name = args.size() > 0 ? args[0].toString() : "";
                juce::String typeStr = args.size() > 1 ? args[1].toString() : "discrete";
                juce::String valuesInput = args.size() > 2 ? args[2].toString() : "0";
                
                ControlType type = (typeStr == "continuous") ? ControlType::CONTINUOUS : ControlType::DISCRETE;
                
                juce::String newId = captureControlManager.addControl(name, type, valuesInput);
                markProjectDirty();
                int stranded = captureControlManager.pruneStrandedKeys();

                auto* resultObj = new juce::DynamicObject();
                resultObj->setProperty("success", true);
                resultObj->setProperty("id", newId);
                resultObj->setProperty("controls", captureControlManager.toVar());
                resultObj->setProperty("totalCaptureCount", captureControlManager.getTotalCaptureCount());
                resultObj->setProperty("strandedExcludedCount", stranded);
                resultObj->setProperty("includedCount", captureControlManager.getIncludedCount());
                
                complete(juce::var(resultObj));
            })
        
        // Remove a capture control by ID
        // Args: [id: string]
        .withNativeFunction(
            "removeCaptureControl",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                if (args.isEmpty())
                {
                    complete(juce::var(false));
                    return;
                }
                
                juce::String id = args[0].toString();
                bool success = captureControlManager.removeControl(id);
                int stranded = 0;
                if (success)
                {
                    markProjectDirty();
                    stranded = captureControlManager.pruneStrandedKeys();
                }

                auto* resultObj = new juce::DynamicObject();
                resultObj->setProperty("success", success);
                resultObj->setProperty("controls", captureControlManager.toVar());
                resultObj->setProperty("totalCaptureCount", captureControlManager.getTotalCaptureCount());
                resultObj->setProperty("strandedExcludedCount", stranded);
                resultObj->setProperty("includedCount", captureControlManager.getIncludedCount());
                
                complete(juce::var(resultObj));
            })
        
        // Update a capture control
        // Args: [id: string, name: string, type: string, valuesInput: string]
        .withNativeFunction(
            "updateCaptureControl",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                if (args.size() < 4)
                {
                    complete(juce::var(false));
                    return;
                }
                
                juce::String id = args[0].toString();
                juce::String name = args[1].toString();
                juce::String typeStr = args[2].toString();
                juce::String valuesInput = args[3].toString();
                
                ControlType type = (typeStr == "continuous") ? ControlType::CONTINUOUS : ControlType::DISCRETE;
                
                bool success = captureControlManager.updateControl(id, name, type, valuesInput);
                int stranded = 0;
                if (success)
                {
                    markProjectDirty();
                    stranded = captureControlManager.pruneStrandedKeys();
                }

                auto* resultObj = new juce::DynamicObject();
                resultObj->setProperty("success", success);
                resultObj->setProperty("controls", captureControlManager.toVar());
                resultObj->setProperty("totalCaptureCount", captureControlManager.getTotalCaptureCount());
                resultObj->setProperty("strandedExcludedCount", stranded);
                resultObj->setProperty("includedCount", captureControlManager.getIncludedCount());
                
                complete(juce::var(resultObj));
            })
        
        // Get total capture count
        .withNativeFunction(
            "getTotalCaptureCount",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(juce::var(captureControlManager.getTotalCaptureCount()));
            })

        // Enumerate all matrix combinations with current include state.
        .withNativeFunction(
            "getMatrixCombinations",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                juce::Array<juce::var> result;
                for (const auto& combo : captureControlManager.getCombinations())
                {
                    auto* obj = new juce::DynamicObject();
                    obj->setProperty("key", combo.key);

                    auto* valuesObj = new juce::DynamicObject();
                    for (int i = 0; i < combo.controlValues.size(); ++i)
                        valuesObj->setProperty(juce::Identifier(combo.controlValues.getAllKeys()[i]),
                                               combo.controlValues.getAllValues()[i]);
                    obj->setProperty("controlValues", juce::var(valuesObj));
                    obj->setProperty("included", !captureControlManager.isExcluded(combo.key));
                    result.add(juce::var(obj));
                }
                complete(juce::var(result));
            })

        // Include/exclude a set of combinations by key.
        // Args: [keys: string[], included: bool]
        .withNativeFunction(
            "setCombinationsIncluded",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                auto* resultObj = new juce::DynamicObject();
                if (args.size() < 2 || !args[0].isArray())
                {
                    resultObj->setProperty("success", false);
                    resultObj->setProperty("error", "requires keys[] and included");
                    complete(juce::var(resultObj));
                    return;
                }

                const bool included = static_cast<bool>(args[1]);
                for (const auto& k : *args[0].getArray())
                    captureControlManager.setExcluded(k.toString(), !included);
                if (!args[0].getArray()->isEmpty())
                    markProjectDirty();

                // Use getCombinations().size() for totalCount rather than
                // getTotalCombinationCount(), which clamps at 100000 even when
                // the product overflows and getCombinations() returns []. Reporting
                // getCombinations().size() keeps includedCount/totalCount consistent
                // with the array that getMatrixCombinations returns to JS.
                // Count in one pass over the same array to avoid a second enumeration.
                const auto combinations = captureControlManager.getCombinations();
                int includedCount = 0;
                for (const auto& combo : combinations)
                    if (!captureControlManager.isExcluded(combo.key))
                        ++includedCount;
                resultObj->setProperty("success", true);
                resultObj->setProperty("includedCount", includedCount);
                resultObj->setProperty("totalCount", combinations.size());
                complete(juce::var(resultObj));
            })

        //==============================================================================
        // Capture List (Generated from Matrix)
        
        // Generate capture list from matrix (cartesian product)
        .withNativeFunction(
            "generateCaptureList",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                captureListManager.generate(captureControlManager);
                markProjectDirty();

                auto* resultObj = new juce::DynamicObject();
                resultObj->setProperty("success", true);
                resultObj->setProperty("captureList", captureListManager.toVar());
                resultObj->setProperty("count", captureListManager.size());
                
                complete(juce::var(resultObj));
            })
        
        // Get capture list
        .withNativeFunction(
            "getCaptureList",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(captureListManager.toVar());
            })
        
        // Clear capture list
        .withNativeFunction(
            "clearCaptureList",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                captureListManager.clear();
                markProjectDirty();
                complete(juce::var(true));
            })
        
        // Set capture item status (for workflow state transitions)
        // Args: [id: string, status: string ("pending", "complete", "failed")]
        .withNativeFunction(
            "setCaptureItemStatus",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                if (args.size() < 2)
                {
                    auto* resultObj = new juce::DynamicObject();
                    resultObj->setProperty("success", false);
                    resultObj->setProperty("error", "requires id and status");
                    complete(juce::var(resultObj));
                    return;
                }
                
                juce::String id = args[0].toString();
                juce::String statusStr = args[1].toString().toLowerCase();
                
                // Map status string to enum
                CaptureStatus newStatus = CaptureStatus::PENDING;
                if (statusStr == "pending")
                    newStatus = CaptureStatus::PENDING;
                else if (statusStr == "complete")
                    newStatus = CaptureStatus::COMPLETE;
                else if (statusStr == "failed")
                    newStatus = CaptureStatus::FAILED;
                else
                {
                    auto* resultObj = new juce::DynamicObject();
                    resultObj->setProperty("success", false);
                    resultObj->setProperty("error", "invalid status");
                    complete(juce::var(resultObj));
                    return;
                }
                
                bool success = captureListManager.setStatus(id, newStatus);
                if (success) markProjectDirty();

                auto* resultObj = new juce::DynamicObject();
                resultObj->setProperty("success", success);
                resultObj->setProperty("captureList", captureListManager.toVar());
                complete(juce::var(resultObj));
            })
        
        // Get expected filename preview for a capture item
        // Args: [captureItemId: string]
        // Returns: { filename, fullPath, hasOutputFolder }
        .withNativeFunction(
            "getExpectedCaptureFilename",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                auto* resultObj = new juce::DynamicObject();
                
                if (args.isEmpty())
                {
                    resultObj->setProperty("filename", "");
                    resultObj->setProperty("fullPath", "");
                    resultObj->setProperty("hasOutputFolder", false);
                    complete(juce::var(resultObj));
                    return;
                }
                
                juce::String captureItemId = args[0].toString();
                CaptureItem* item = captureListManager.findById(captureItemId);
                
                if (item == nullptr)
                {
                    resultObj->setProperty("filename", "");
                    resultObj->setProperty("fullPath", "");
                    resultObj->setProperty("hasOutputFolder", false);
                    complete(juce::var(resultObj));
                    return;
                }
                
                juce::String filename = CaptureFilenameGenerator::generateFilename(
                    *item,
                    captureControlManager.getControls(),
                    audioEngine.getReferenceSignalPath(),
                    audioEngine.getCurrentSampleRate());
                bool hasOutputFolder = outputFolder.exists() && outputFolder.isDirectory() && isOutputFolderWritable();
                
                resultObj->setProperty("filename", filename);
                
                if (hasOutputFolder)
                {
                    resultObj->setProperty("fullPath", outputFolder.getChildFile(filename).getFullPathName());
                }
                else
                {
                    resultObj->setProperty("fullPath", "");
                }
                
                resultObj->setProperty("hasOutputFolder", hasOutputFolder);
                
                complete(juce::var(resultObj));
            })
        
        //==============================================================================
        // Monitor Output Routing
        
        // Get current monitor state
        .withNativeFunction(
            "getMonitorState",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(getMonitorStateVar());
            })
        
        // Set monitor output device
        // Args: [deviceName: string] - empty string to disable
        .withNativeFunction(
            "setMonitorDevice",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                juce::String deviceName = args.isEmpty() ? "" : args[0].toString();
                bool success = audioEngine.setMonitorDevice(deviceName);
                complete(juce::var(success));
            })
        
        // Set monitor output channel configuration
        // Args: [channelIndex: number, stereo: boolean]
        .withNativeFunction(
            "setMonitorChannel",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                if (args.isEmpty())
                {
                    complete(juce::var(false));
                    return;
                }
                
                int channelIndex = static_cast<int>(args[0]);
                bool stereo = args.size() > 1 ? static_cast<bool>(args[1]) : true;
                
                audioEngine.setMonitorChannel(channelIndex, stereo);
                complete(juce::var(true));
            })
        
        // Set monitor source (what to monitor: "none", "input", "output")
        // Args: [source: string]
        .withNativeFunction(
            "setMonitorSource",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                if (args.isEmpty())
                {
                    complete(juce::var(false));
                    return;
                }
                
                juce::String sourceStr = args[0].toString().toLowerCase();
                AudioEngine::MonitorSource source = AudioEngine::MonitorSource::NONE;
                
                if (sourceStr == "input")
                    source = AudioEngine::MonitorSource::INPUT;
                else if (sourceStr == "output")
                    source = AudioEngine::MonitorSource::OUTPUT;
                
                audioEngine.setMonitorSource(source);
                complete(juce::var(true));
            })
        
        // Set monitor gain/level in dB
        // Args: [gainDb: number]
        .withNativeFunction(
            "setMonitorGain",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                float gainDb = args.isEmpty() ? -6.0f : static_cast<float>(args[0]);
                audioEngine.setMonitorGain(gainDb);
                complete(juce::var(audioEngine.getMonitorGain()));
            })
        
        // Get monitor gain in dB
        .withNativeFunction(
            "getMonitorGain",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(juce::var(audioEngine.getMonitorGain()));
            })
        
        //==============================================================================
        // Project Save/Load
        
        // Save project to a file (opens file picker dialog)
        // Returns: { success: boolean, filePath?: string, errorMessage?: string, cancelled?: boolean }
        .withNativeFunction(
            "saveProjectAs",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                // Determine default directory: prefer output folder, then current project location, then documents
                juce::File defaultDir;
                if (outputFolder.exists() && outputFolder.isDirectory())
                    defaultDir = outputFolder;
                else if (currentProjectFile.existsAsFile())
                    defaultDir = currentProjectFile.getParentDirectory();
                else
                    defaultDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

#if REFCAP_E2E
                // e2e automation: save to an explicit path one level below the
                // chooser. Only honored under the e2e driver (--e2e-test-port).
                if (E2EBridge::isRequested() && !args.isEmpty() && args[0].isString())
                {
                    auto file = juce::File(args[0].toString());
                    if (!file.hasFileExtension(".rcp") && !file.hasFileExtension(".json"))
                        file = file.withFileExtension(".rcp");

                    auto projectData = serializeProjectState();
                    auto jsonString = juce::JSON::toString(projectData, true);

                    auto* resultObj = new juce::DynamicObject();
                    resultObj->setProperty("cancelled", false);
                    if (file.replaceWithText(jsonString))
                    {
                        currentProjectFile = file;
                        if (!outputFolder.exists() || !outputFolder.isDirectory())
                            outputFolder = file.getParentDirectory();
                        resultObj->setProperty("success", true);
                        resultObj->setProperty("filePath", file.getFullPathName());
                        resultObj->setProperty("fileName", file.getFileName());
                        resultObj->setProperty("outputFolderPath", outputFolder.getFullPathName());
                    }
                    else
                    {
                        resultObj->setProperty("success", false);
                        resultObj->setProperty("errorMessage", "Failed to write project file");
                    }
                    complete(juce::var(resultObj));
                    return;
                }
#else
                juce::ignoreUnused(args);
#endif

                // Create file chooser for saving project
                fileChooser = std::make_unique<juce::FileChooser>(
                    "Save Project As",
                    defaultDir,
                    "*.rcp;*.json"
                );
                
                // Launch async save dialog
                auto chooserFlags = juce::FileBrowserComponent::saveMode 
                    | juce::FileBrowserComponent::canSelectFiles
                    | juce::FileBrowserComponent::warnAboutOverwriting;
                
                fileChooser->launchAsync(chooserFlags, [this, complete](const juce::FileChooser& fc) {
                    auto results = fc.getResults();
                    
                    if (results.isEmpty())
                    {
                        // User cancelled
                        auto* resultObj = new juce::DynamicObject();
                        resultObj->setProperty("cancelled", true);
                        resultObj->setProperty("success", false);
                        complete(juce::var(resultObj));
                        return;
                    }
                    
                    auto file = results.getFirst();
                    
                    // Ensure file has an extension
                    if (!file.hasFileExtension(".rcp") && !file.hasFileExtension(".json"))
                    {
                        file = file.withFileExtension(".rcp");
                    }
                    
                    // Serialize project state
                    auto projectData = serializeProjectState();
                    auto jsonString = juce::JSON::toString(projectData, true);
                    
                    // Write to file
                    auto* resultObj = new juce::DynamicObject();
                    
                    if (file.replaceWithText(jsonString))
                    {
                        // Update current project file
                        currentProjectFile = file;
                        
                        // If no output folder is set, default to the project's folder
                        if (!outputFolder.exists() || !outputFolder.isDirectory())
                        {
                            outputFolder = file.getParentDirectory();
                        }
                        
                        resultObj->setProperty("success", true);
                        resultObj->setProperty("cancelled", false);
                        resultObj->setProperty("filePath", file.getFullPathName());
                        resultObj->setProperty("fileName", file.getFileName());
                        resultObj->setProperty("outputFolderPath", outputFolder.getFullPathName());
                    }
                    else
                    {
                        resultObj->setProperty("success", false);
                        resultObj->setProperty("cancelled", false);
                        resultObj->setProperty("errorMessage", "Failed to write project file");
                    }
                    
                    complete(juce::var(resultObj));
                });
            })
        
        // Get current project file path (if saved)
        .withNativeFunction(
            "getCurrentProjectPath",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                auto* resultObj = new juce::DynamicObject();
                
                if (currentProjectFile.existsAsFile())
                {
                    resultObj->setProperty("hasSavedProject", true);
                    resultObj->setProperty("filePath", currentProjectFile.getFullPathName());
                    resultObj->setProperty("fileName", currentProjectFile.getFileName());
                }
                else
                {
                    resultObj->setProperty("hasSavedProject", false);
                    resultObj->setProperty("filePath", "");
                    resultObj->setProperty("fileName", "");
                }
                
                complete(juce::var(resultObj));
            })
        
        // Load project from a file (opens file picker dialog)
        // Returns: { success: boolean, filePath?: string, fileName?: string, errorMessage?: string, 
        //            cancelled?: boolean, referenceSignalMissing?: boolean, missingReferenceSignalPath?: string }
        .withNativeFunction(
            "loadProject",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                // Shared load path for both the chooser callback and the e2e
                // path-injection route below.
                auto loadFromFile = [this, complete](const juce::File& file) {
                    // Read and parse the project file
                    auto jsonString = file.loadFileAsString();
                    auto projectData = juce::JSON::parse(jsonString);

                    if (!projectData.isObject())
                    {
                        auto* resultObj = new juce::DynamicObject();
                        resultObj->setProperty("cancelled", false);
                        resultObj->setProperty("success", false);
                        resultObj->setProperty("errorMessage", "Invalid project file: could not parse JSON");
                        complete(juce::var(resultObj));
                        return;
                    }

                    // Deserialize on the message thread to avoid CoreAudio threading issues
                    juce::MessageManager::callAsync([this, complete, file, projectData]() {
                        // Point currentProjectFile at the file being opened *before*
                        // deserializing: deserializeProjectState() runs a post-load
                        // autosave, and it must target this file rather than fall
                        // through to a fabricated project.rcp. Restore on failure.
                        auto previousProjectFile = currentProjectFile;
                        currentProjectFile = file;

                        // Deserialize the project state
                        auto loadResult = deserializeProjectState(projectData);

                        auto* resultObj = new juce::DynamicObject();
                        resultObj->setProperty("cancelled", false);

                        if (loadResult.success)
                        {
                            resultObj->setProperty("success", true);
                            resultObj->setProperty("filePath", file.getFullPathName());
                            resultObj->setProperty("fileName", file.getFileName());
                            
                            // Include reference signal warning if applicable
                            if (loadResult.referenceSignalMissing)
                            {
                                resultObj->setProperty("referenceSignalMissing", true);
                                resultObj->setProperty("missingReferenceSignalPath", loadResult.missingReferenceSignalPath);
                            }
                            else
                            {
                                resultObj->setProperty("referenceSignalMissing", false);
                            }
                        }
                        else
                        {
                            currentProjectFile = previousProjectFile;
                            resultObj->setProperty("success", false);
                            resultObj->setProperty("errorMessage", loadResult.errorMessage);
                        }

                        complete(juce::var(resultObj));
                    });
                };

#if REFCAP_E2E
                // e2e automation: native file choosers cannot be driven by the
                // test harness, so allow injecting an explicit path one level
                // below the dialog. Only honored when launched by the e2e
                // driver (--e2e-test-port).
                if (E2EBridge::isRequested() && !args.isEmpty() && args[0].isString())
                {
                    auto file = juce::File(args[0].toString());

                    if (!file.existsAsFile())
                    {
                        auto* resultObj = new juce::DynamicObject();
                        resultObj->setProperty("cancelled", false);
                        resultObj->setProperty("success", false);
                        resultObj->setProperty("errorMessage", "Project file not found: " + file.getFullPathName());
                        complete(juce::var(resultObj));
                        return;
                    }

                    loadFromFile(file);
                    return;
                }
#else
                juce::ignoreUnused(args);
#endif

                // Determine default directory: prefer output folder, then current project location, then documents
                juce::File defaultDir;
                if (outputFolder.exists() && outputFolder.isDirectory())
                    defaultDir = outputFolder;
                else if (currentProjectFile.existsAsFile())
                    defaultDir = currentProjectFile.getParentDirectory();
                else
                    defaultDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

                // Create file chooser for opening project
                fileChooser = std::make_unique<juce::FileChooser>(
                    "Open Project",
                    defaultDir,
                    "*.rcp;*.json"
                );

                // Launch async open dialog
                auto chooserFlags = juce::FileBrowserComponent::openMode
                    | juce::FileBrowserComponent::canSelectFiles;

                fileChooser->launchAsync(chooserFlags, [complete, loadFromFile](const juce::FileChooser& fc) {
                    auto results = fc.getResults();

                    if (results.isEmpty())
                    {
                        // User cancelled
                        auto* resultObj = new juce::DynamicObject();
                        resultObj->setProperty("cancelled", true);
                        resultObj->setProperty("success", false);
                        complete(juce::var(resultObj));
                        return;
                    }

                    loadFromFile(results.getFirst());
                });
            })

        // Create new project (reset all state to defaults)
        // Returns: { success: boolean }
        .withNativeFunction(
            "newProject",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                // Reset capture controls and list
                captureControlManager.clear();
                captureListManager.clear();
                
                // Reset calibration state
                calibrationState.reset();
                audioEngine.setOutputGainTrim(0.0f);
                
                // Clear reference signal
                audioEngine.clearReferenceSignal();
                
                // Clear reference signals
                referenceSignals.clear();
                selectedPreviewSignalId = juce::String();
                currentCaptureSignalIndex = 0;
                
                // Reset output folder and capture log
                outputFolder = juce::File();
                captureLogManager.reset();
                
                // Clear current project file
                currentProjectFile = juce::File();
                
                auto* resultObj = new juce::DynamicObject();
                resultObj->setProperty("success", true);
                complete(juce::var(resultObj));
            });
}

std::optional<juce::WebBrowserComponent::Resource> MainComponent::resourceProvider(const juce::String& path)
{
    auto webappDir = getWebappDirectory();
    
    // Handle root path - serve index.html
    auto requestPath = (path == "/" || path.isEmpty()) ? "index.html" : path;
    
    // Remove leading slash if present
    if (requestPath.startsWithChar('/'))
        requestPath = requestPath.substring(1);
    
    auto file = webappDir.getChildFile(requestPath);
    
    if (!file.existsAsFile())
        return std::nullopt;
    
    juce::MemoryBlock fileData;
    if (!file.loadFileAsData(fileData))
        return std::nullopt;
    
    juce::WebBrowserComponent::Resource resource;
    resource.mimeType = getMimeType(requestPath);
    resource.data.resize(fileData.getSize());
    std::memcpy(resource.data.data(), fileData.getData(), fileData.getSize());
    
    return resource;
}

juce::File MainComponent::getWebappDirectory() const
{
    // Get the application bundle directory
    auto appFile = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
    
#if JUCE_MAC
    // On macOS, resources are in the app bundle's Resources folder
    auto resourcesDir = appFile.getChildFile("Contents/Resources/webapp");
    if (resourcesDir.isDirectory())
        return resourcesDir;
#endif
    
    // Fallback: Look for Resources/webapp relative to the executable (development)
    auto devDir = appFile.getParentDirectory().getParentDirectory().getParentDirectory()
                         .getChildFile("Resources/webapp");
    if (devDir.isDirectory())
        return devDir;
    
    // Final fallback: current working directory
    return juce::File::getCurrentWorkingDirectory().getChildFile("Resources/webapp");
}

juce::String MainComponent::getMimeType(const juce::String& path) const
{
    auto extension = path.fromLastOccurrenceOf(".", false, true).toLowerCase();
    
    if (extension == "html" || extension == "htm")
        return "text/html";
    if (extension == "css")
        return "text/css";
    if (extension == "js")
        return "application/javascript";
    if (extension == "json")
        return "application/json";
    if (extension == "png")
        return "image/png";
    if (extension == "jpg" || extension == "jpeg")
        return "image/jpeg";
    if (extension == "svg")
        return "image/svg+xml";
    if (extension == "woff")
        return "font/woff";
    if (extension == "woff2")
        return "font/woff2";
    
    return "application/octet-stream";
}

//==============================================================================
// Native function helpers

juce::var MainComponent::intArrayToVar(const juce::Array<int>& ints) const
{
    juce::Array<juce::var> result;
    
    for (const auto& val : ints)
    {
        result.add(juce::var(val));
    }
    
    return juce::var(result);
}

juce::var MainComponent::getAudioStateVar() const
{
    auto* obj = new juce::DynamicObject();
    
    obj->setProperty("inputDevice", audioEngine.getCurrentInputDevice());
    obj->setProperty("inputChannel", audioEngine.getCurrentInputChannel());
    obj->setProperty("outputDevice", audioEngine.getCurrentOutputDevice());
    obj->setProperty("outputChannel", audioEngine.getCurrentOutputChannel());
    
    return juce::var(obj);
}

//==============================================================================
// Reference Signals helpers

juce::String MainComponent::generateSignalId() const
{
    auto timestamp = juce::Time::getMillisecondCounter();
    auto random = juce::Random::getSystemRandom().nextInt();
    return "sig_" + juce::String::toHexString(timestamp) + juce::String::toHexString(random);
}

juce::var MainComponent::getReferenceSignalsVar() const
{
    juce::Array<juce::var> signalsArray;
    
    for (const auto& signal : referenceSignals)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("id", signal.id);
        obj->setProperty("filePath", signal.filePath);
        obj->setProperty("fileName", signal.fileName);
        obj->setProperty("sampleRate", signal.sampleRate);
        obj->setProperty("numSamples", signal.numSamples);
        obj->setProperty("durationSeconds", signal.durationSeconds);
        obj->setProperty("tailMs", signal.tailMs);
        signalsArray.add(juce::var(obj));
    }
    
    auto* result = new juce::DynamicObject();
    result->setProperty("signals", juce::var(signalsArray));
    result->setProperty("selectedId", selectedPreviewSignalId);
    result->setProperty("isPlaying", audioEngine.isReferencePlaybackActive());
    result->setProperty("isLooping", audioEngine.isReferencePlaybackLooping());
    
    return juce::var(result);
}

ReferenceSignal* MainComponent::findSignalById(const juce::String& id)
{
    for (auto& signal : referenceSignals)
    {
        if (signal.id == id)
            return &signal;
    }
    return nullptr;
}

const ReferenceSignal* MainComponent::findSignalById(const juce::String& id) const
{
    for (const auto& signal : referenceSignals)
    {
        if (signal.id == id)
            return &signal;
    }
    return nullptr;
}

bool MainComponent::validateSignalSampleRate(int signalSampleRate) const
{
    return signalSampleRate == audioEngine.getCurrentSampleRate();
}

juce::var MainComponent::addReferenceSignalFromPath(const juce::String& filePath)
{
    auto* resultObj = new juce::DynamicObject();
    
    juce::File file(filePath);
    
    if (!file.existsAsFile())
    {
        resultObj->setProperty("success", false);
        resultObj->setProperty("errorMessage", "File not found");
        return juce::var(resultObj);
    }
    
    if (!file.hasFileExtension(".wav"))
    {
        resultObj->setProperty("success", false);
        resultObj->setProperty("errorMessage", "Only WAV files are supported");
        return juce::var(resultObj);
    }
    
    // Load and validate the file
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    
    if (reader == nullptr)
    {
        resultObj->setProperty("success", false);
        resultObj->setProperty("errorMessage", "Could not read WAV file");
        return juce::var(resultObj);
    }
    
    // Check mono
    if (reader->numChannels != 1)
    {
        resultObj->setProperty("success", false);
        resultObj->setProperty("errorMessage", "Only mono WAV files are supported");
        return juce::var(resultObj);
    }
    
    // Check sample rate matches session
    int signalSampleRate = static_cast<int>(reader->sampleRate);
    if (!validateSignalSampleRate(signalSampleRate))
    {
        resultObj->setProperty("success", false);
        resultObj->setProperty("errorMessage", 
            "Sample rate mismatch: file is " + juce::String(signalSampleRate) + 
            " Hz but session is " + juce::String(audioEngine.getCurrentSampleRate()) + " Hz");
        return juce::var(resultObj);
    }
    
    // Create signal entry
    ReferenceSignal signal;
    signal.id = generateSignalId();
    signal.filePath = file.getFullPathName();
    signal.fileName = file.getFileName();
    signal.sampleRate = signalSampleRate;
    signal.numSamples = static_cast<int>(reader->lengthInSamples);
    signal.durationSeconds = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
    signal.tailMs = 500; // Default tail
    
    referenceSignals.add(signal);
    markProjectDirty();

    resultObj->setProperty("success", true);
    resultObj->setProperty("id", signal.id);
    resultObj->setProperty("signal", signal.toVar());

    return juce::var(resultObj);
}

//==============================================================================
// Monitor state helpers

juce::var MainComponent::getMonitorStateVar() const
{
    auto* obj = new juce::DynamicObject();
    
    obj->setProperty("device", audioEngine.getMonitorDevice());
    obj->setProperty("channel", audioEngine.getMonitorChannel());
    obj->setProperty("stereo", audioEngine.isMonitorStereo());
    obj->setProperty("gainDb", audioEngine.getMonitorGain());
    obj->setProperty("active", audioEngine.isMonitorActive());
    
    // Convert source enum to string
    auto source = audioEngine.getMonitorSource();
    juce::String sourceStr = "none";
    if (source == AudioEngine::MonitorSource::INPUT)
        sourceStr = "input";
    else if (source == AudioEngine::MonitorSource::OUTPUT)
        sourceStr = "output";
    obj->setProperty("source", sourceStr);
    
    return juce::var(obj);
}

//==============================================================================
// Calibration state helpers

juce::var MainComponent::getCalibrationStateVar() const
{
    return calibrationState.toVar();
}

//==============================================================================
// Timer callback

void MainComponent::timerCallback()
{
    // Get metering values from audio engine
    auto inputMeters = audioEngine.getInputMeterValues();
    auto outputMeters = audioEngine.getOutputMeterValues();
    auto monitorMeters = audioEngine.getMonitorMeterValues();
    
    // Build metering data object
    auto* meterData = new juce::DynamicObject();
    
    auto* inputObj = new juce::DynamicObject();
    inputObj->setProperty("rmsDb", inputMeters.rmsDb);
    inputObj->setProperty("peakDb", inputMeters.peakDb);
    inputObj->setProperty("rmsHoldDb", inputMeters.rmsHoldDb);
    inputObj->setProperty("peakHoldDb", inputMeters.peakHoldDb);
    
    auto* outputObj = new juce::DynamicObject();
    outputObj->setProperty("rmsDb", outputMeters.rmsDb);
    outputObj->setProperty("peakDb", outputMeters.peakDb);
    outputObj->setProperty("rmsHoldDb", outputMeters.rmsHoldDb);
    outputObj->setProperty("peakHoldDb", outputMeters.peakHoldDb);
    
    auto* monitorObj = new juce::DynamicObject();
    monitorObj->setProperty("rmsDb", monitorMeters.rmsDb);
    monitorObj->setProperty("peakDb", monitorMeters.peakDb);
    monitorObj->setProperty("rmsHoldDb", monitorMeters.rmsHoldDb);
    monitorObj->setProperty("peakHoldDb", monitorMeters.peakHoldDb);
    
    meterData->setProperty("input", juce::var(inputObj));
    meterData->setProperty("output", juce::var(outputObj));
    meterData->setProperty("monitor", juce::var(monitorObj));
    
    // Emit to frontend
    emitEvent("meterUpdate", juce::var(meterData));
    
    // Flush a debounced auto-save if the project was recently marked dirty and
    // has settled (no further changes within the debounce window). This coalesces
    // rapid changes (slider drags, quick edits) into a single file write.
    if (projectDirty
        && (juce::Time::getMillisecondCounter() - lastDirtyMs) >= autoSaveDebounceMs)
    {
        projectDirty = false;
        autoSaveProject();
    }

    // Check for playback state changes (e.g., playback finished at end of file)
    bool currentPlaybackState = audioEngine.isReferencePlaybackActive();
    if (currentPlaybackState != lastPlaybackState)
    {
        lastPlaybackState = currentPlaybackState;
        
        auto* playbackData = new juce::DynamicObject();
        playbackData->setProperty("isPlaying", currentPlaybackState);
        emitEvent("playbackStateChanged", juce::var(playbackData));
    }
}

//==============================================================================
// AudioEngine::Listener callback

void MainComponent::audioDeviceListChanged()
{
    // Emit event to frontend so it can refresh device lists
    emitEvent("audioDevicesChanged", juce::var());
}

//==============================================================================
// AudioEngine::CaptureListener callbacks

void MainComponent::captureStateChanged(AudioEngine::CaptureState newState)
{
    auto* stateData = new juce::DynamicObject();
    
    juce::String stateStr;
    switch (newState)
    {
        case AudioEngine::CaptureState::IDLE: stateStr = "idle"; break;
        case AudioEngine::CaptureState::RECORDING: stateStr = "recording"; break;
        case AudioEngine::CaptureState::DONE: stateStr = "done"; break;
    }
    
    stateData->setProperty("state", stateStr);
    
    // Include total duration for progress calculation
    // Get tail from current signal if available
    int currentTailMs = 500; // default
    if (currentCaptureSignalIndex >= 0 && currentCaptureSignalIndex < referenceSignals.size())
    {
        currentTailMs = referenceSignals[currentCaptureSignalIndex].tailMs;
    }
    
    double refDuration = audioEngine.getReferenceSignalDuration();
    double totalDurationMs = (refDuration * 1000.0) + 50.0 + static_cast<double>(currentTailMs);
    stateData->setProperty("totalDurationMs", totalDurationMs);
    
    // Include signal info for multi-signal progress display
    stateData->setProperty("signalIndex", currentCaptureSignalIndex);
    stateData->setProperty("signalCount", referenceSignals.size());
    if (currentCaptureSignalIndex >= 0 && currentCaptureSignalIndex < referenceSignals.size())
    {
        stateData->setProperty("signalName", referenceSignals[currentCaptureSignalIndex].fileName);
    }
    
    // Auto-save when recording starts (to persist the "recording" state)
    if (newState == AudioEngine::CaptureState::RECORDING && outputFolder.exists())
    {
        autoSaveProject();
    }
    
    emitEvent("captureStateChanged", juce::var(stateData));
}

void MainComponent::captureComplete(const AudioEngine::CaptureResult& result)
{
    auto* completeData = new juce::DynamicObject();
    
    completeData->setProperty("success", result.success);
    completeData->setProperty("errorMessage", result.errorMessage);
    completeData->setProperty("outputFilePath", result.outputFilePath);
    completeData->setProperty("durationSeconds", result.durationSeconds);
    
    // Include signal progress info
    completeData->setProperty("signalIndex", currentCaptureSignalIndex);
    completeData->setProperty("signalCount", referenceSignals.size());
    
    // Log this signal capture if successful
    if (result.success && outputFolder.exists())
    {
        // Get current capture item
        CaptureItem* captureItem = currentCaptureItemId.isNotEmpty() 
            ? captureListManager.findById(currentCaptureItemId) : nullptr;
        
        if (captureItem != nullptr)
        {
            // Add this output file path to the capture item
            captureItem->outputFilePaths.add(result.outputFilePath);
            
            // Get tail from current signal
            int currentTailMs = 500;
            if (currentCaptureSignalIndex >= 0 && currentCaptureSignalIndex < referenceSignals.size())
            {
                currentTailMs = referenceSignals[currentCaptureSignalIndex].tailMs;
            }
            
            // Get level info from the audio engine (max levels during capture)
            auto inputMeters = audioEngine.getInputMeterValues();
            captureLogManager.appendCapture(
                *captureItem,
                audioEngine.getReferenceSignalPath(),
                audioEngine.getCurrentSampleRate(),
                currentTailMs,
                result.durationSeconds,
                inputMeters.peakHoldDb,
                inputMeters.rmsHoldDb);
        }
        
        // Check if there are more signals to capture
        bool hasMoreSignals = (currentCaptureSignalIndex + 1) < referenceSignals.size();
        completeData->setProperty("hasMoreSignals", hasMoreSignals);
        
        if (hasMoreSignals)
        {
            // Advance to next signal and auto-start capture
            currentCaptureSignalIndex++;
            
            // Emit the completion event before starting next capture
            emitEvent("captureComplete", juce::var(completeData));
            
            // Start next signal capture after a small delay (to let UI update)
            juce::MessageManager::callAsync([this]() {
                startNextSignalCapture();
            });
            return;
        }
        else
        {
            // All signals captured - mark capture item as complete
            if (captureItem != nullptr)
            {
                captureItem->status = CaptureStatus::COMPLETE;
            }
            
            // Reset state for next capture item
            currentCaptureItemId = juce::String();
            currentCaptureSignalIndex = 0;
        }
        
        // Auto-save project to output folder
        autoSaveProject();
    }
    else if (!result.success)
    {
        // Capture failed - reset state
        currentCaptureItemId = juce::String();
        currentCaptureSignalIndex = 0;
    }
    
    emitEvent("captureComplete", juce::var(completeData));
}

void MainComponent::startNextSignalCapture()
{
    if (currentCaptureItemId.isEmpty() || referenceSignals.isEmpty())
        return;
    
    if (currentCaptureSignalIndex < 0 || currentCaptureSignalIndex >= referenceSignals.size())
        return;
    
    CaptureItem* captureItem = captureListManager.findById(currentCaptureItemId);
    if (captureItem == nullptr)
        return;
    
    const auto& signal = referenceSignals.getReference(currentCaptureSignalIndex);
    
    // Load the signal into audio engine
    juce::File signalFile(signal.filePath);
    if (!signalFile.existsAsFile())
    {
        DBG("Signal file not found: " + signal.filePath);
        return;
    }
    
    auto loadResult = audioEngine.loadReferenceSignal(signalFile);
    if (!loadResult.success)
    {
        DBG("Failed to load signal: " + loadResult.errorMessage);
        return;
    }
    
    // Generate filename for this signal
    juce::String filename = CaptureFilenameGenerator::generateFilename(
        *captureItem,
        captureControlManager.getControls(),
        signal.filePath,
        audioEngine.getCurrentSampleRate());
    
    juce::File outputFile = outputFolder.getChildFile(filename);
    
    // Start the capture
    bool success = audioEngine.startCapture(outputFile, signal.tailMs);
    
    if (!success)
    {
        DBG("Failed to start capture for signal: " + signal.fileName);
        
        // Emit error event
        auto* errorData = new juce::DynamicObject();
        errorData->setProperty("success", false);
        errorData->setProperty("errorMessage", "Failed to start capture for signal: " + signal.fileName);
        emitEvent("captureComplete", juce::var(errorData));
        
        // Reset state
        currentCaptureItemId = juce::String();
        currentCaptureSignalIndex = 0;
    }
}

//==============================================================================
// Output folder helpers

bool MainComponent::isOutputFolderWritable() const
{
    if (!outputFolder.exists() || !outputFolder.isDirectory())
        return false;
    
    // Try to create a test file to verify write permission
    auto testFile = outputFolder.getChildFile(".write_test_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    
    if (testFile.create().wasOk())
    {
        testFile.deleteFile();
        return true;
    }
    
    return false;
}

juce::var MainComponent::getOutputFolderStateVar() const
{
    auto* obj = new juce::DynamicObject();
    
    if (outputFolder.exists() && outputFolder.isDirectory())
    {
        obj->setProperty("path", outputFolder.getFullPathName());
        obj->setProperty("isWritable", isOutputFolderWritable());
    }
    else
    {
        obj->setProperty("path", "");
        obj->setProperty("isWritable", false);
    }
    
    return juce::var(obj);
}



//==============================================================================
// Project State Serialization

juce::var MainComponent::serializeProjectState() const
{
    auto* projectObj = new juce::DynamicObject();
    
    // Version for future compatibility
    projectObj->setProperty("version", "1.0");
    
    //--------------------------------------------------------------------------
    // Audio Settings - use ProjectSerializer helper
    ProjectAudioSettings audioSettings;
    audioSettings.inputDevice = audioEngine.getCurrentInputDevice();
    audioSettings.inputChannel = audioEngine.getCurrentInputChannel();
    audioSettings.outputDevice = audioEngine.getCurrentOutputDevice();
    audioSettings.outputChannel = audioEngine.getCurrentOutputChannel();
    audioSettings.sampleRate = audioEngine.getCurrentSampleRate();
    projectObj->setProperty("audioSettings", ProjectSerializer::serializeAudioSettings(audioSettings));
    
    //--------------------------------------------------------------------------
    // Calibration - use CalibrationState's toVar()
    projectObj->setProperty("calibration", calibrationState.toVar());
    
    //--------------------------------------------------------------------------
    // Reference Signals - use ReferenceSignalSerializer
    projectObj->setProperty("referenceSignals", ReferenceSignalSerializer::serialize(referenceSignals));
    
    //--------------------------------------------------------------------------
    // Capture Settings - use ProjectSerializer helper (tailMs no longer stored here, it's per-signal)
    ProjectCaptureSettings captureSettings;
    captureSettings.tailMs = 500; // Default value, not used anymore
    captureSettings.outputFolderPath = outputFolder.getFullPathName();
    projectObj->setProperty("captureSettings", ProjectSerializer::serializeCaptureSettings(captureSettings));
    
    //--------------------------------------------------------------------------
    // Matrix (Controls + exclusions) - use ProjectSerializer helper, then attach
    // the matrix-level excludedKeys set.
    auto matrixVar = ProjectSerializer::serializeControls(captureControlManager.getControls());
    if (auto* matrixObj = matrixVar.getDynamicObject())
        matrixObj->setProperty("excludedKeys",
                               ProjectSerializer::stringArrayToVar(captureControlManager.getExcludedKeys()));
    projectObj->setProperty("matrix", matrixVar);
    
    //--------------------------------------------------------------------------
    // Captures (List with status) - use ProjectSerializer helper
    projectObj->setProperty("captures", ProjectSerializer::serializeCaptureList(captureListManager.getItems()));
    
    //--------------------------------------------------------------------------
    // Visual Guide State (pass-through from frontend)
    if (!guideState.isVoid())
    {
        projectObj->setProperty("guideState", guideState);
    }
    
    return juce::var(projectObj);
}

LoadProjectResult MainComponent::deserializeProjectState(const juce::var& projectData)
{
    LoadProjectResult result;
    result.success = false;
    
    if (!projectData.isObject())
    {
        result.errorMessage = "Invalid project file format";
        return result;
    }
    
    auto* projectObj = projectData.getDynamicObject();
    if (projectObj == nullptr)
    {
        result.errorMessage = "Invalid project file format";
        return result;
    }
    
    // Check version (for future compatibility)
    auto version = projectObj->getProperty("version").toString();
    if (version.isEmpty())
    {
        result.errorMessage = "Missing version in project file";
        return result;
    }
    
    //--------------------------------------------------------------------------
    // Audio Settings - use ProjectSerializer helper for parsing
    int projectSampleRate = 48000; // Default
    auto audioSettingsVar = projectObj->getProperty("audioSettings");
    if (audioSettingsVar.isObject())
    {
        auto audioSettings = ProjectSerializer::deserializeAudioSettings(audioSettingsVar);
        
        // Store project sample rate for reference signal validation
        if (audioSettings.sampleRate > 0)
            projectSampleRate = audioSettings.sampleRate;
        
        // Get available devices to validate
        auto availableInputDevices = audioEngine.getInputDeviceNames();
        auto availableOutputDevices = audioEngine.getOutputDeviceNames();
        
        // Apply audio settings only if devices exist
        if (audioSettings.inputDevice.isNotEmpty() && availableInputDevices.contains(audioSettings.inputDevice))
            audioEngine.setInputDevice(audioSettings.inputDevice, audioSettings.inputChannel);
        
        if (audioSettings.outputDevice.isNotEmpty() && availableOutputDevices.contains(audioSettings.outputDevice))
            audioEngine.setOutputDevice(audioSettings.outputDevice, audioSettings.outputChannel);
        
        if (audioSettings.sampleRate > 0)
            audioEngine.setSampleRate(audioSettings.sampleRate);
    }
    
    //--------------------------------------------------------------------------
    // Calibration - use CalibrationState::fromVar()
    auto calibrationVar = projectObj->getProperty("calibration");
    if (calibrationVar.isObject())
    {
        calibrationState = CalibrationState::fromVar(calibrationVar);
        
        // Apply output trim to audio engine
        audioEngine.setOutputGainTrim(calibrationState.outputTrimDb);
    }
    
    //--------------------------------------------------------------------------
    // Reference Signals - use ReferenceSignalSerializer
    referenceSignals.clear();
    selectedPreviewSignalId = juce::String();
    currentCaptureSignalIndex = 0;
    audioEngine.clearReferenceSignal();
    
    auto referenceSignalsVar = projectObj->getProperty("referenceSignals");
    if (referenceSignalsVar.isArray())
    {
        auto loadedSignals = ReferenceSignalSerializer::deserialize(referenceSignalsVar);
        
        for (const auto& signal : loadedSignals)
        {
            juce::File signalFile(signal.filePath);
            
            if (signalFile.existsAsFile())
            {
                // Validate sample rate matches project's sample rate setting
                // (use project sample rate, not current device which may not be initialized)
                juce::AudioFormatManager formatManager;
                formatManager.registerBasicFormats();
                
                std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(signalFile));
                if (reader != nullptr)
                {
                    if (static_cast<int>(reader->sampleRate) == projectSampleRate)
                    {
                        referenceSignals.add(signal);
                    }
                    else
                    {
                        // Sample rate mismatch - skip but note as warning
                        result.referenceSignalMissing = true;
                        result.missingReferenceSignalPath = signal.filePath + " (sample rate mismatch)";
                    }
                }
            }
            else
            {
                // Reference signal file doesn't exist
                result.referenceSignalMissing = true;
                result.missingReferenceSignalPath = signal.filePath;
            }
        }
    }
    
    //--------------------------------------------------------------------------
    // Capture Settings - use ProjectSerializer helper for parsing (tailMs no longer used)
    auto captureSettingsVar = projectObj->getProperty("captureSettings");
    if (captureSettingsVar.isObject())
    {
        auto captureSettings = ProjectSerializer::deserializeCaptureSettings(captureSettingsVar);
        
        if (captureSettings.outputFolderPath.isNotEmpty())
        {
            juce::File folder(captureSettings.outputFolderPath);
            if (folder.exists() && folder.isDirectory())
            {
                outputFolder = folder;
                // Reset capture log state for loaded folder
                captureLogManager.setOutputFolder(folder);
            }
        }
    }
    
    //--------------------------------------------------------------------------
    // Matrix (Controls) - use captureControlManager.addControl() for proper parsing
    captureControlManager.clear();
    
    auto matrixVar = projectObj->getProperty("matrix");
    if (matrixVar.isObject())
    {
        auto* matrix = matrixVar.getDynamicObject();
        if (matrix != nullptr)
        {
            auto controlsVar = matrix->getProperty("controls");
            if (auto* controlsArray = controlsVar.getArray())
            {
                for (const auto& ctrlVar : *controlsArray)
                {
                    if (auto* ctrlObj = ctrlVar.getDynamicObject())
                    {
                        auto name = ctrlObj->getProperty("name").toString();
                        auto typeStr = ctrlObj->getProperty("type").toString();
                        ControlType type = (typeStr == "continuous") ? ControlType::CONTINUOUS : ControlType::DISCRETE;
                        
                        // Build values string from array
                        juce::String valuesStr;
                        auto valuesVar = ctrlObj->getProperty("values");
                        if (auto* valuesArray = valuesVar.getArray())
                        {
                            for (int i = 0; i < valuesArray->size(); ++i)
                            {
                                if (i > 0) valuesStr += ",";
                                valuesStr += (*valuesArray)[i].toString();
                            }
                        }
                        
                        // Add control using manager (it will parse values)
                        captureControlManager.addControl(name, type, valuesStr);
                    }
                }
            }

            // Restore matrix-level exclusions (empty for legacy projects).
            juce::StringArray excluded;
            auto excludedVar = matrix->getProperty("excludedKeys");
            if (auto* excludedArray = excludedVar.getArray())
                for (const auto& v : *excludedArray)
                    excluded.add(v.toString());
            captureControlManager.setExcludedKeys(excluded);
            captureControlManager.pruneStrandedKeys();
        }
    }

    //--------------------------------------------------------------------------
    // Captures (List with status) - use ProjectSerializer helper with ID generator
    captureListManager.clear();
    
    auto capturesVar = projectObj->getProperty("captures");
    auto generateCaptureId = []() {
        auto timestamp = juce::Time::getMillisecondCounter();
        auto random = juce::Random::getSystemRandom().nextInt();
        return "cap_" + juce::String::toHexString(timestamp) + juce::String::toHexString(random);
    };
    
    auto loadedItems = ProjectSerializer::deserializeCaptureList(capturesVar, generateCaptureId);
    for (auto& item : loadedItems)
    {
        captureListManager.getItemsRef().add(item);
    }
    
    //--------------------------------------------------------------------------
    // Visual Guide State (pass-through to frontend)
    auto guideStateVar = projectObj->getProperty("guideState");
    if (!guideStateVar.isVoid())
    {
        guideState = guideStateVar;
    }
    else
    {
        guideState = juce::var(); // Clear if not in project file
    }
    
    // Auto-save after load to persist any status changes (e.g., ready -> pending)
    if (outputFolder.exists())
        autoSaveProject();
    
    result.success = true;
    return result;
}

void MainComponent::autoSaveProject()
{
    // Only autosave to an explicitly-saved project file. If there is no named
    // project yet, do nothing rather than fabricating a project.rcp next to the
    // output folder: that silent fallback littered stray files and, worse, could
    // become the file everything subsequently autosaved to. A named project is
    // established by saveProjectAs or by opening an existing file.
    if (!currentProjectFile.existsAsFile())
        return;

    juce::File projectFile = currentProjectFile;

    // Serialize project state
    auto projectData = serializeProjectState();
    auto jsonString = juce::JSON::toString(projectData, true);

    // Write to file (silent, no error reporting to UI)
    if (projectFile.replaceWithText(jsonString))
    {
        DBG("Auto-saved project to: " + projectFile.getFullPathName());

        // Notify the frontend so it can show a "saved" indicator + timestamp.
        auto* savedObj = new juce::DynamicObject();
        savedObj->setProperty("path", projectFile.getFullPathName());
        savedObj->setProperty("fileName", projectFile.getFileName());
        savedObj->setProperty("timeMs", (juce::int64) juce::Time::getCurrentTime().toMilliseconds());
        emitEvent("projectSaved", juce::var(savedObj));
    }
    else
    {
        DBG("Failed to auto-save project to: " + projectFile.getFullPathName());
    }
}

void MainComponent::markProjectDirty()
{
    projectDirty = true;
    lastDirtyMs = juce::Time::getMillisecondCounter();
}

//==============================================================================
// Menu Actions

void MainComponent::performMenuAction(const juce::String& action)
{
    if (action == "newProject")
    {
        // Reset state first
        captureControlManager.clear();
        captureListManager.clear();
        calibrationState.reset();
        audioEngine.setOutputGainTrim(0.0f);
        audioEngine.clearReferenceSignal();
        referenceSignals.clear();
        selectedPreviewSignalId = juce::String();
        currentCaptureSignalIndex = 0;
        outputFolder = juce::File();
        captureLogManager.reset();
        currentProjectFile = juce::File();
        
        // Notify frontend to reset UI and open save dialog
        emitEvent("projectNewRequested", juce::var());
    }
    else if (action == "openProject")
    {
        // Determine default directory: prefer output folder, then current project location, then documents
        juce::File defaultDir;
        if (outputFolder.exists() && outputFolder.isDirectory())
            defaultDir = outputFolder;
        else if (currentProjectFile.existsAsFile())
            defaultDir = currentProjectFile.getParentDirectory();
        else
            defaultDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        
        // Create file chooser for opening project
        fileChooser = std::make_unique<juce::FileChooser>(
            "Open Project",
            defaultDir,
            "*.rcp;*.json"
        );
        
        // Launch async open dialog
        auto chooserFlags = juce::FileBrowserComponent::openMode 
            | juce::FileBrowserComponent::canSelectFiles;
        
        fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc) {
            auto results = fc.getResults();
            
            if (results.isEmpty())
            {
                // User cancelled - no action needed
                return;
            }
            
            auto file = results.getFirst();
            
            // Read and parse the project file
            auto jsonString = file.loadFileAsString();
            auto projectData = juce::JSON::parse(jsonString);
            
            if (!projectData.isObject())
            {
                // Show error (could enhance with alert dialog)
                DBG("Failed to parse project file: " + file.getFullPathName());
                return;
            }
            
            // Deserialize on the message thread to avoid CoreAudio threading issues
            juce::MessageManager::callAsync([this, file, projectData]() {
                // Point currentProjectFile at the file being opened *before*
                // deserializing so the post-load autosave targets it (see the
                // loadProject native function for the full rationale).
                auto previousProjectFile = currentProjectFile;
                currentProjectFile = file;

                // Deserialize the project state
                auto loadResult = deserializeProjectState(projectData);

                auto* resultObj = new juce::DynamicObject();

                if (loadResult.success)
                {
                    resultObj->setProperty("success", true);
                    resultObj->setProperty("filePath", file.getFullPathName());
                    resultObj->setProperty("fileName", file.getFileName());
                    
                    // Include reference signal warning if applicable
                    if (loadResult.referenceSignalMissing)
                    {
                        resultObj->setProperty("referenceSignalMissing", true);
                        resultObj->setProperty("missingReferenceSignalPath", loadResult.missingReferenceSignalPath);
                    }
                }
                else
                {
                    currentProjectFile = previousProjectFile;
                    resultObj->setProperty("success", false);
                    resultObj->setProperty("errorMessage", loadResult.errorMessage);
                }

                // Notify frontend about the loaded project
                emitEvent("projectLoaded", juce::var(resultObj));
            });
        });
    }
}
