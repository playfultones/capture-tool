#pragma once

#include <juce_core/juce_core.h>
#include "../capture/CaptureControl.h"
#include "../capture/CaptureList.h"

/**
 * Calibration state data.
 * 
 * Stores the results of the calibration process, including
 * measured levels and any trim adjustments.
 */
struct CalibrationState
{
    bool completed = false;
    juce::String completedAt;
    float testToneLevelDbfs = -18.0f;
    float unityLevelDbfs = -100.0f;
    float maxLevelDbfs = -100.0f;
    float outputTrimDb = 0.0f;  // Output trim applied to test tone and reference playback
    
    /**
     * Reset to default values.
     */
    void reset()
    {
        completed = false;
        completedAt = juce::String();
        testToneLevelDbfs = -18.0f;
        unityLevelDbfs = -100.0f;
        maxLevelDbfs = -100.0f;
        outputTrimDb = 0.0f;
    }
    
    /**
     * Convert to juce::var for JSON serialization.
     */
    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("completed", completed);
        obj->setProperty("completedAt", completedAt);
        obj->setProperty("testToneLevelDbfs", testToneLevelDbfs);
        obj->setProperty("unityLevelDbfs", unityLevelDbfs);
        obj->setProperty("maxLevelDbfs", maxLevelDbfs);
        obj->setProperty("outputTrimDb", outputTrimDb);
        return juce::var(obj);
    }
    
    /**
     * Load from juce::var (JSON deserialization).
     */
    static CalibrationState fromVar(const juce::var& data)
    {
        CalibrationState state;
        
        if (auto* obj = data.getDynamicObject())
        {
            state.completed = static_cast<bool>(obj->getProperty("completed"));
            state.completedAt = obj->getProperty("completedAt").toString();
            state.testToneLevelDbfs = static_cast<float>(obj->getProperty("testToneLevelDbfs"));
            state.unityLevelDbfs = static_cast<float>(obj->getProperty("unityLevelDbfs"));
            state.maxLevelDbfs = static_cast<float>(obj->getProperty("maxLevelDbfs"));
            state.outputTrimDb = static_cast<float>(obj->getProperty("outputTrimDb"));
        }
        
        return state;
    }
};

/**
 * Result of loading a project file.
 */
struct LoadProjectResult
{
    bool success = false;
    juce::String errorMessage;
    bool referenceSignalMissing = false;
    juce::String missingReferenceSignalPath;
};

/**
 * Audio settings from/for project serialization.
 * 
 * Used as an intermediate representation for audio device settings
 * since AudioEngine operations may need to validate against available devices.
 */
struct ProjectAudioSettings
{
    juce::String inputDevice;
    int inputChannel = 0;
    juce::String outputDevice;
    int outputChannel = 0;
    int sampleRate = 48000;
};

/**
 * Capture settings from/for project serialization.
 */
struct ProjectCaptureSettings
{
    int tailMs = 500;
    juce::String outputFolderPath;
};

/**
 * Reference signal info from/for project serialization.
 */
struct ProjectReferenceSignal
{
    juce::String path;
    int durationMs = 0;
};

/**
 * Helper functions for project state serialization/deserialization.
 * 
 * These functions handle conversion between the in-memory project state
 * and JSON format for saving/loading project files (.rcp).
 */
namespace ProjectSerializer
{
    /**
     * Convert a StringArray to juce::var array.
     */
    inline juce::var stringArrayToVar(const juce::StringArray& strings)
    {
        juce::Array<juce::var> result;
        for (const auto& str : strings)
            result.add(juce::var(str));
        return juce::var(result);
    }

    /**
     * Serialize audio settings to juce::var.
     */
    juce::var serializeAudioSettings(const ProjectAudioSettings& settings);

    /**
     * Deserialize audio settings from juce::var.
     */
    ProjectAudioSettings deserializeAudioSettings(const juce::var& data);

    /**
     * Serialize capture settings to juce::var.
     */
    juce::var serializeCaptureSettings(const ProjectCaptureSettings& settings);

    /**
     * Deserialize capture settings from juce::var.
     */
    ProjectCaptureSettings deserializeCaptureSettings(const juce::var& data);

    /**
     * Serialize reference signal info to juce::var.
     */
    juce::var serializeReferenceSignal(const ProjectReferenceSignal& signal);

    /**
     * Deserialize reference signal info from juce::var.
     */
    ProjectReferenceSignal deserializeReferenceSignal(const juce::var& data);

    /**
     * Serialize capture controls (matrix) to juce::var.
     */
    juce::var serializeControls(const juce::Array<CaptureControl>& controls);

    /**
     * Deserialize capture controls from juce::var.
     * 
     * @param data The JSON data
     * @param generateId Function to generate unique IDs for controls
     * @return Array of CaptureControl objects
     */
    juce::Array<CaptureControl> deserializeControls(const juce::var& data,
                                                    std::function<juce::String()> generateId);

    /**
     * Serialize capture list to juce::var.
     */
    juce::var serializeCaptureList(const juce::Array<CaptureItem>& items);

    /**
     * Deserialize capture list from juce::var.
     * 
     * @param data The JSON data
     * @param generateId Function to generate unique IDs for items
     * @return Array of CaptureItem objects
     */
    juce::Array<CaptureItem> deserializeCaptureList(const juce::var& data,
                                                    std::function<juce::String()> generateId);

}
