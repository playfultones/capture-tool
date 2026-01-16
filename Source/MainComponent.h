#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
#include "AudioEngine.h"
#include "capture/CaptureControl.h"
#include "capture/CaptureList.h"
#include "capture/CaptureFilename.h"
#include "capture/CaptureLog.h"
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
    // Reference signal helpers

    /** Get reference signal state as JSON for frontend */
    juce::var getReferenceSignalStateVar() const;

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
     *  Saves to project.rcp in the output folder
     */
    void autoSaveProject();

    /** Current project file path (empty if not saved) */
    juce::File currentProjectFile;

    //==============================================================================
    // Recording tail configuration

    /** Recording tail duration in milliseconds (0, 250, 500, 1000) */
    int recordingTailMs = 500;

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
