#pragma once

#include <juce_core/juce_core.h>

/**
 * Represents a reference signal in the capture session.
 * 
 * Each session can have multiple reference signals. During capture,
 * each matrix entry will cycle through all signals, recording each
 * one sequentially with its configured tail duration.
 */
struct ReferenceSignal
{
    /** Unique identifier for this signal */
    juce::String id;
    
    /** Full path to the WAV file */
    juce::String filePath;
    
    /** Display name (filename without path) */
    juce::String fileName;
    
    /** Sample rate of the WAV file (must match session sample rate) */
    int sampleRate = 0;
    
    /** Number of samples in the file */
    int numSamples = 0;
    
    /** Duration in seconds */
    double durationSeconds = 0.0;
    
    /** Recording tail duration in milliseconds (0, 250, 500, 1000) */
    int tailMs = 500;
    
    /**
     * Convert to juce::var for JSON serialization.
     */
    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("id", id);
        obj->setProperty("filePath", filePath);
        obj->setProperty("fileName", fileName);
        obj->setProperty("sampleRate", sampleRate);
        obj->setProperty("numSamples", numSamples);
        obj->setProperty("durationSeconds", durationSeconds);
        obj->setProperty("tailMs", tailMs);
        return juce::var(obj);
    }
    
    /**
     * Load from juce::var (JSON deserialization).
     */
    static ReferenceSignal fromVar(const juce::var& data)
    {
        ReferenceSignal signal;
        
        if (auto* obj = data.getDynamicObject())
        {
            signal.id = obj->getProperty("id").toString();
            signal.filePath = obj->getProperty("filePath").toString();
            signal.fileName = obj->getProperty("fileName").toString();
            signal.sampleRate = static_cast<int>(obj->getProperty("sampleRate"));
            signal.numSamples = static_cast<int>(obj->getProperty("numSamples"));
            signal.durationSeconds = static_cast<double>(obj->getProperty("durationSeconds"));
            signal.tailMs = static_cast<int>(obj->getProperty("tailMs"));
            
            // Default tail to 500ms if not specified or invalid
            if (signal.tailMs != 0 && signal.tailMs != 250 && 
                signal.tailMs != 500 && signal.tailMs != 1000)
            {
                signal.tailMs = 500;
            }
        }
        
        return signal;
    }
};

/**
 * Helper functions for reference signal array serialization.
 */
namespace ReferenceSignalSerializer
{
    /**
     * Serialize an array of reference signals to juce::var.
     */
    inline juce::var serialize(const juce::Array<ReferenceSignal>& signals)
    {
        juce::Array<juce::var> result;
        for (const auto& signal : signals)
        {
            result.add(signal.toVar());
        }
        return juce::var(result);
    }
    
    /**
     * Deserialize an array of reference signals from juce::var.
     */
    inline juce::Array<ReferenceSignal> deserialize(const juce::var& data)
    {
        juce::Array<ReferenceSignal> signals;
        
        if (auto* array = data.getArray())
        {
            for (const auto& item : *array)
            {
                signals.add(ReferenceSignal::fromVar(item));
            }
        }
        
        return signals;
    }
}
