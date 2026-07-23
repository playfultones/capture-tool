#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
#include "AudioEngine.h"
#include "capture/CaptureControl.h"
#include "capture/CaptureList.h"
#include "capture/CaptureFilename.h"
#include "capture/CaptureLog.h"
#include "capture/ReferenceSignal.h"
#include "project/ProjectState.h"

class MainComponent : public juce::Component,
                      private juce::Timer,
                      private AudioEngine::Listener,
                      private AudioEngine::CaptureListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void resized() override;

    /** Emit an event to the frontend */
    void emitEvent(const juce::Identifier& eventId, const juce::var& data);
    
    /** Handle menu actions from the native menu bar */
    void performMenuAction(const juce::String& action);

private:
    /** Create WebBrowserComponent options with native integration */
    juce::WebBrowserComponent::Options createWebViewOptions();
    
    /** Resource provider for serving webapp files */
    std::optional<juce::WebBrowserComponent::Resource> resourceProvider(const juce::String& path);
    
    /** Get the webapp resources directory path */
    juce::File getWebappDirectory() const;
    
    /** Determine MIME type from file extension */
    juce::String getMimeType(const juce::String& path) const;
    
    //==============================================================================
    // Native function handlers for WebView
    
    /** Convert int array to JSON array for frontend */
    juce::var intArrayToVar(const juce::Array<int>& ints) const;
    
    /** Get current audio state as JSON for frontend */
    juce::var getAudioStateVar() const;

    //==============================================================================
    // Timer callback for metering updates
    
    void timerCallback() override;

    //==============================================================================
    // AudioEngine::Listener callback
    
    void audioDeviceListChanged() override;

    //==============================================================================
    // AudioEngine::CaptureListener callbacks
    
    void captureStateChanged(AudioEngine::CaptureState newState) override;
    void captureComplete(const AudioEngine::CaptureResult& result) override;

    //==============================================================================
    // Reference Signals (Multiple per session)

    /** List of reference signals for this session */
    juce::Array<ReferenceSignal> referenceSignals;

    /** Currently selected signal ID for preview playback */
    juce::String selectedPreviewSignalId;

    /** Current signal index during multi-signal capture (0-based) */
    int currentCaptureSignalIndex = 0;

    /** Current capture item ID for multi-signal orchestration */
    juce::String currentCaptureItemId;

    /** Start capture for the next signal in the sequence */
    void startNextSignalCapture();

    /** Generate a unique ID for a new reference signal */
    juce::String generateSignalId() const;

    /** Get reference signals as JSON array for frontend */
    juce::var getReferenceSignalsVar() const;

    /** Find a reference signal by ID, returns nullptr if not found */
    ReferenceSignal* findSignalById(const juce::String& id);
    const ReferenceSignal* findSignalById(const juce::String& id) const;

    /** Add a reference signal from file path. Returns result with success/error. */
    juce::var addReferenceSignalFromPath(const juce::String& filePath);

    /** Validate that a signal matches the session sample rate */
    bool validateSignalSampleRate(int signalSampleRate) const;

    //==============================================================================
    // Monitor state helpers

    /** Get monitor state as JSON for frontend */
    juce::var getMonitorStateVar() const;

    //==============================================================================
    // Calibration state (using CalibrationState from project/ProjectState.h)
    
    CalibrationState calibrationState;

    /** Get calibration state as JSON for frontend */
    juce::var getCalibrationStateVar() const;

    //==============================================================================
    // Capture Controls (Matrix Definition) - using CaptureControlManager

    /** Capture control manager for matrix definition */
    CaptureControlManager captureControlManager;

    //==============================================================================
    // Capture List (Generated from Matrix) - using CaptureListManager

    /** Capture list manager for tracking captures */
    CaptureListManager captureListManager;

    //==============================================================================
    // Auto-export filename generation - using CaptureFilenameGenerator
    // (CaptureFilenameGenerator provides static methods, no instance needed)

    //==============================================================================
    // Capture Log Generation - using CaptureLogManager

    /** Capture log manager for recording capture metadata */
    CaptureLogManager captureLogManager;

    //==============================================================================
    // Project State Serialization (using LoadProjectResult from project/ProjectState.h)

    /** Serialize entire project state to JSON object
     *  Used for saving project files (.rcp or .json)
     */
    juce::var serializeProjectState() const;

    /** Deserialize project state from JSON object
     *  Returns a result indicating success or failure
     */
    LoadProjectResult deserializeProjectState(const juce::var& projectData);

    /** Auto-save project to output folder (silent, no dialog)
     *  Saves to the current project file (or project.rcp in the output folder).
     *  Emits a "projectSaved" event to the frontend on success.
     */
    void autoSaveProject();

    /** Mark the project as having unsaved changes. A debounced auto-save is
     *  flushed from the timer callback shortly after the last change, so rapid
     *  changes (e.g. slider drags) coalesce into a single write.
     */
    void markProjectDirty();

    /** Current project file path (empty if not saved) */
    juce::File currentProjectFile;

    /** Debounced auto-save state */
    bool projectDirty = false;
    juce::uint32 lastDirtyMs = 0;
    static constexpr juce::uint32 autoSaveDebounceMs = 400;

    //==============================================================================
    // Visual Guide State (stored as JSON from frontend)

    /** Guide state from frontend visual guide module.
     *  Stored as-is (pass-through) - the frontend manages the structure.
     */
    juce::var guideState;

    //==============================================================================
    // Output folder configuration

    /** Output folder for captured WAV files */
    juce::File outputFolder;

    /** Check if output folder is writable */
    bool isOutputFolderWritable() const;

    /** Get output folder state as JSON for frontend */
    juce::var getOutputFolderStateVar() const;

    //==============================================================================
    // Members
    
    AudioEngine audioEngine;
    std::unique_ptr<juce::WebBrowserComponent> webView;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Track last playback state to detect changes
    bool lastPlaybackState = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
