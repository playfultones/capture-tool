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
 * that will be captured.
 */
struct CaptureItem
{
    juce::String id;                          // Unique identifier (UUID)
    int index = 0;                            // Index in the capture list (1-based for display)
    juce::StringPairArray controlValues;      // Control name -> value mapping
    CaptureStatus status = CaptureStatus::PENDING;
    juce::String outputFilePath;              // Path to captured file (when complete)
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
     * Set the output file path of a capture item.
     * 
     * @param id The item ID
     * @param path Output file path
     * @return true if found and updated
     */
    bool setOutputPath(const juce::String& id, const juce::String& path);

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
