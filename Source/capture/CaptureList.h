#pragma once

#include <juce_core/juce_core.h>
#include "CaptureControl.h"

/**
 * Status of a capture item in the list.
 */
enum class CaptureStatus
{
    PENDING,    // Not yet captured - awaiting user to dial in settings
    COMPLETE,   // Successfully captured
    FAILED      // Capture failed
};

/**
 * A single capture item in the generated list.
 * 
 * Represents one specific combination of control values
 * that will be captured. When multiple reference signals are used,
 * each signal produces its own output file, all stored in outputFilePaths.
 */
struct CaptureItem
{
    juce::String id;                          // Unique identifier (UUID)
    int index = 0;                            // Index in the capture list (1-based for display)
    juce::StringPairArray controlValues;      // Control name -> value mapping
    CaptureStatus status = CaptureStatus::PENDING;
    juce::StringArray outputFilePaths;        // Paths to captured files (one per reference signal)
    bool isRoundtrip = false;                 // True for roundtrip entry (unit bypassed, no control values)
    
    // Legacy compatibility - returns first output path or empty string
    juce::String getOutputFilePath() const 
    { 
        return outputFilePaths.isEmpty() ? juce::String() : outputFilePaths[0]; 
    }
};

/**
 * Manager for the capture list.
 * 
 * The capture list is generated from the capture controls as a
 * cartesian product of all control values. Each item in the list
 * represents one specific capture to perform.
 */
class CaptureListManager
{
public:
    CaptureListManager() = default;

    /**
     * Generate the capture list from controls.
     * Creates the cartesian product of all control values.
     * 
     * @param controlManager The controls to generate from
     */
    void generate(const CaptureControlManager& controlManager);

    /**
     * Clear the capture list.
     */
    void clear() { items.clear(); }

    /**
     * Get all items.
     */
    const juce::Array<CaptureItem>& getItems() const { return items; }

    /**
     * Get mutable access to items.
     */
    juce::Array<CaptureItem>& getItemsRef() { return items; }

    /**
     * Get number of items.
     */
    int size() const { return items.size(); }

    /**
     * Check if list is empty.
     */
    bool isEmpty() const { return items.isEmpty(); }

    /**
     * Find a capture item by ID.
     * 
     * @param id The item ID to find
     * @return Pointer to item, or nullptr if not found
     */
    CaptureItem* findById(const juce::String& id);

    /**
     * Set the status of a capture item.
     * 
     * @param id The item ID
     * @param status New status
     * @return true if found and updated
     */
    bool setStatus(const juce::String& id, CaptureStatus status);

    /**
     * Add an output file path to a capture item.
     * Used when capturing multiple reference signals per item.
     * 
     * @param id The item ID
     * @param path Output file path to add
     * @return true if found and updated
     */
    bool addOutputPath(const juce::String& id, const juce::String& path);
    
    /**
     * Clear output file paths for a capture item.
     * Called when starting a new capture for this item.
     * 
     * @param id The item ID
     * @return true if found and cleared
     */
    bool clearOutputPaths(const juce::String& id);

    /**
     * Convert capture list to juce::var for JSON serialization.
     */
    juce::var toVar() const;

    /**
     * Convert CaptureStatus to string.
     */
    static juce::String statusToString(CaptureStatus status);

    /**
     * Parse CaptureStatus from string.
     */
    static CaptureStatus stringToStatus(const juce::String& str);

private:
    juce::Array<CaptureItem> items;

    /** Generate a unique ID for a capture item */
    static juce::String generateItemId();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CaptureListManager)
};
