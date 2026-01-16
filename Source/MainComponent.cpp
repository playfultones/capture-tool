#include "MainComponent.h"





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
                complete(juce::var(success));
            })
        
        // Browse for reference signal file (opens native file picker)
        .withNativeFunction(
            "browseReferenceSignal",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                // Create file chooser for WAV files
                fileChooser = std::make_unique<juce::FileChooser>(
                    "Select Reference Signal",
                    juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                    "*.wav"
                );
                
                // Launch async file picker
                auto chooserFlags = juce::FileBrowserComponent::openMode 
                    | juce::FileBrowserComponent::canSelectFiles;
                
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
                    
                    auto file = results.getFirst();
                    auto loadResult = audioEngine.loadReferenceSignal(file);
                    
                    auto* resultObj = new juce::DynamicObject();
                    resultObj->setProperty("cancelled", false);
                    resultObj->setProperty("success", loadResult.success);
                    
                    if (loadResult.success)
                    {
                        resultObj->setProperty("filePath", file.getFullPathName());
                        resultObj->setProperty("fileName", file.getFileName());
                        resultObj->setProperty("sampleRate", loadResult.sampleRate);
                        resultObj->setProperty("numSamples", loadResult.numSamples);
                        resultObj->setProperty("durationSeconds", loadResult.durationSeconds);
                    }
                    else
                    {
                        resultObj->setProperty("errorMessage", loadResult.errorMessage);
                    }
                    
                    complete(juce::var(resultObj));
                });
            })
        
        // Get current reference signal state
        .withNativeFunction(
            "getReferenceSignalState",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(getReferenceSignalStateVar());
            })
        
        // Clear/unload the reference signal
        .withNativeFunction(
            "clearReferenceSignal",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                audioEngine.clearReferenceSignal();
                complete(juce::var(true));
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
        
        // Get recording tail duration in milliseconds
        .withNativeFunction(
            "getRecordingTailMs",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(juce::var(recordingTailMs));
            })
        
        // Set recording tail duration in milliseconds
        // Args: [tailMs: number] - one of 0, 250, 500, 1000
        .withNativeFunction(
            "setRecordingTailMs",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                if (args.isEmpty())
                {
                    complete(juce::var(recordingTailMs));
                    return;
                }
                
                int newTailMs = static_cast<int>(args[0]);
                
                // Validate: only allow specific values
                if (newTailMs == 0 || newTailMs == 250 || newTailMs == 500 || newTailMs == 1000)
                {
                    recordingTailMs = newTailMs;
                }
                
                complete(juce::var(recordingTailMs));
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
        // Args: [captureItemId: string, tailMs: number (optional, uses configured recordingTailMs)]
        // If captureItemId is provided, uses output folder + standardized naming
        // If no captureItemId, falls back to temp directory
        .withNativeFunction(
            "startCapture",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                // Parse arguments
                juce::String captureItemId = args.size() > 0 ? args[0].toString() : "";
                int tailMs = args.size() > 1 ? static_cast<int>(args[1]) : recordingTailMs;
                
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
                    
                    // Update the capture item's output file path
                    if (captureItem != nullptr)
                    {
                        captureItem->outputFilePath = outputFile.getFullPathName();
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
                
                auto* resultObj = new juce::DynamicObject();
                resultObj->setProperty("success", true);
                resultObj->setProperty("id", newId);
                resultObj->setProperty("controls", captureControlManager.toVar());
                resultObj->setProperty("totalCaptureCount", captureControlManager.getTotalCaptureCount());
                
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
                
                auto* resultObj = new juce::DynamicObject();
                resultObj->setProperty("success", success);
                resultObj->setProperty("controls", captureControlManager.toVar());
                resultObj->setProperty("totalCaptureCount", captureControlManager.getTotalCaptureCount());
                
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
                
                auto* resultObj = new juce::DynamicObject();
                resultObj->setProperty("success", success);
                resultObj->setProperty("controls", captureControlManager.toVar());
                resultObj->setProperty("totalCaptureCount", captureControlManager.getTotalCaptureCount());
                
                complete(juce::var(resultObj));
            })
        
        // Get total capture count
        .withNativeFunction(
            "getTotalCaptureCount",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                complete(juce::var(captureControlManager.getTotalCaptureCount()));
            })
        
        //==============================================================================
        // Capture List (Generated from Matrix)
        
        // Generate capture list from matrix (cartesian product)
        .withNativeFunction(
            "generateCaptureList",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                captureListManager.generate(captureControlManager);
                
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
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                // Determine default directory: prefer output folder, then current project location, then documents
                juce::File defaultDir;
                if (outputFolder.exists() && outputFolder.isDirectory())
                    defaultDir = outputFolder;
                else if (currentProjectFile.existsAsFile())
                    defaultDir = currentProjectFile.getParentDirectory();
                else
                    defaultDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
                
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
                        
                        resultObj->setProperty("success", true);
                        resultObj->setProperty("cancelled", false);
                        resultObj->setProperty("filePath", file.getFullPathName());
                        resultObj->setProperty("fileName", file.getFileName());
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
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
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
                        // Deserialize the project state
                        auto loadResult = deserializeProjectState(projectData);
                        
                        auto* resultObj = new juce::DynamicObject();
                        resultObj->setProperty("cancelled", false);
                        
                        if (loadResult.success)
                        {
                            // Update current project file
                            currentProjectFile = file;
                            
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
                            resultObj->setProperty("success", false);
                            resultObj->setProperty("errorMessage", loadResult.errorMessage);
                        }
                        
                        complete(juce::var(resultObj));
                    });
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
                
                // Reset output folder and capture log
                outputFolder = juce::File();
                captureLogManager.reset();
                
                // Reset recording tail to default
                recordingTailMs = 500;
                
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

juce::var MainComponent::getReferenceSignalStateVar() const
{
    auto* obj = new juce::DynamicObject();
    
    bool hasSignal = audioEngine.hasReferenceSignal();
    obj->setProperty("loaded", hasSignal);
    
    if (hasSignal)
    {
        auto filePath = audioEngine.getReferenceSignalPath();
        obj->setProperty("filePath", filePath);
        obj->setProperty("fileName", juce::File(filePath).getFileName());
        obj->setProperty("sampleRate", audioEngine.getReferenceSignalSampleRate());
        obj->setProperty("numSamples", audioEngine.getReferenceSignalNumSamples());
        obj->setProperty("durationSeconds", audioEngine.getReferenceSignalDuration());
    }
    
    // Include playback state
    obj->setProperty("isPlaying", audioEngine.isReferencePlaybackActive());
    obj->setProperty("playbackPosition", audioEngine.getReferencePlaybackPosition());
    obj->setProperty("isLooping", audioEngine.isReferencePlaybackLooping());
    
    return juce::var(obj);
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
    
    auto* outputObj = new juce::DynamicObject();
    outputObj->setProperty("rmsDb", outputMeters.rmsDb);
    outputObj->setProperty("peakDb", outputMeters.peakDb);
    
    auto* monitorObj = new juce::DynamicObject();
    monitorObj->setProperty("rmsDb", monitorMeters.rmsDb);
    monitorObj->setProperty("peakDb", monitorMeters.peakDb);
    
    meterData->setProperty("input", juce::var(inputObj));
    meterData->setProperty("output", juce::var(outputObj));
    meterData->setProperty("monitor", juce::var(monitorObj));
    
    // Emit to frontend
    emitEvent("meterUpdate", juce::var(meterData));
    
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
    double refDuration = audioEngine.getReferenceSignalDuration();
    double totalDurationMs = (refDuration * 1000.0) + 50.0 + static_cast<double>(recordingTailMs);
    stateData->setProperty("totalDurationMs", totalDurationMs);
    
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
    
    // Update capture status and log if successful
    if (result.success && outputFolder.exists())
    {
        // Find the capture item by output path and update its status
        for (auto& item : captureListManager.getItemsRef())
        {
            if (item.outputFilePath == result.outputFilePath)
            {
                // Mark as complete in backend state
                item.status = CaptureStatus::COMPLETE;
                
                // Get level info from the audio engine (last capture levels)
                auto inputMeters = audioEngine.getInputMeterValues();
                captureLogManager.appendCapture(
                    item,
                    audioEngine.getReferenceSignalPath(),
                    audioEngine.getCurrentSampleRate(),
                    recordingTailMs,
                    result.durationSeconds,
                    inputMeters.peakDb,
                    inputMeters.rmsDb);
                break;
            }
        }
        
        // Auto-save project to output folder (now with updated status)
        autoSaveProject();
    }
    
    emitEvent("captureComplete", juce::var(completeData));
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
    // Reference Signal - use ProjectSerializer helper
    ProjectReferenceSignal refSignal;
    if (audioEngine.hasReferenceSignal())
    {
        refSignal.path = audioEngine.getReferenceSignalPath();
        refSignal.durationMs = static_cast<int>(audioEngine.getReferenceSignalDuration() * 1000.0);
    }
    projectObj->setProperty("referenceSignal", ProjectSerializer::serializeReferenceSignal(refSignal));
    
    //--------------------------------------------------------------------------
    // Capture Settings - use ProjectSerializer helper
    ProjectCaptureSettings captureSettings;
    captureSettings.tailMs = recordingTailMs;
    captureSettings.outputFolderPath = outputFolder.getFullPathName();
    projectObj->setProperty("captureSettings", ProjectSerializer::serializeCaptureSettings(captureSettings));
    
    //--------------------------------------------------------------------------
    // Matrix (Controls) - use ProjectSerializer helper
    projectObj->setProperty("matrix", ProjectSerializer::serializeControls(captureControlManager.getControls()));
    
    //--------------------------------------------------------------------------
    // Captures (List with status) - use ProjectSerializer helper
    projectObj->setProperty("captures", ProjectSerializer::serializeCaptureList(captureListManager.getItems()));
    
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
    auto audioSettingsVar = projectObj->getProperty("audioSettings");
    if (audioSettingsVar.isObject())
    {
        auto audioSettings = ProjectSerializer::deserializeAudioSettings(audioSettingsVar);
        
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
    // Reference Signal - use ProjectSerializer helper for parsing
    auto referenceSignalVar = projectObj->getProperty("referenceSignal");
    if (referenceSignalVar.isObject())
    {
        auto refSignal = ProjectSerializer::deserializeReferenceSignal(referenceSignalVar);
        
        if (refSignal.path.isNotEmpty())
        {
            juce::File refFile(refSignal.path);
            
            if (refFile.existsAsFile())
            {
                auto loadResult = audioEngine.loadReferenceSignal(refFile);
                if (!loadResult.success)
                {
                    // Non-fatal: reference signal failed to load but continue loading project
                    result.referenceSignalMissing = true;
                    result.missingReferenceSignalPath = refSignal.path;
                }
            }
            else
            {
                // Reference signal file doesn't exist
                result.referenceSignalMissing = true;
                result.missingReferenceSignalPath = refSignal.path;
            }
        }
    }
    
    //--------------------------------------------------------------------------
    // Capture Settings - use ProjectSerializer helper for parsing
    auto captureSettingsVar = projectObj->getProperty("captureSettings");
    if (captureSettingsVar.isObject())
    {
        auto captureSettings = ProjectSerializer::deserializeCaptureSettings(captureSettingsVar);
        
        if (captureSettings.tailMs == 0 || captureSettings.tailMs == 250 || 
            captureSettings.tailMs == 500 || captureSettings.tailMs == 1000)
            recordingTailMs = captureSettings.tailMs;
        
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
    
    // Auto-save after load to persist any status changes (e.g., ready -> pending)
    if (outputFolder.exists())
        autoSaveProject();
    
    result.success = true;
    return result;
}

void MainComponent::autoSaveProject()
{
    // Only auto-save if we have a valid output folder
    if (!outputFolder.exists() || !outputFolder.isDirectory())
        return;
    
    // Save to project.rcp in the output folder
    auto projectFile = outputFolder.getChildFile("project.rcp");
    
    // Serialize project state
    auto projectData = serializeProjectState();
    auto jsonString = juce::JSON::toString(projectData, true);
    
    // Write to file (silent, no error reporting to UI)
    if (projectFile.replaceWithText(jsonString))
    {
        // Update current project file reference
        currentProjectFile = projectFile;
        DBG("Auto-saved project to: " + projectFile.getFullPathName());
    }
    else
    {
        DBG("Failed to auto-save project to: " + projectFile.getFullPathName());
    }
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
        outputFolder = juce::File();
        captureLogManager.reset();
        recordingTailMs = 500;
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
                // Deserialize the project state
                auto loadResult = deserializeProjectState(projectData);
                
                auto* resultObj = new juce::DynamicObject();
                
                if (loadResult.success)
                {
                    // Update current project file
                    currentProjectFile = file;
                    
                    // Debug logging
                    juce::File logFile(juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("rc_debug.log"));
                    logFile.appendText("openProject menu: set currentProjectFile to: " + currentProjectFile.getFullPathName() + "\n");
                    
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
                    resultObj->setProperty("success", false);
                    resultObj->setProperty("errorMessage", loadResult.errorMessage);
                }
                
                // Notify frontend about the loaded project
                emitEvent("projectLoaded", juce::var(resultObj));
            });
        });
    }
}
