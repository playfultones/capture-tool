#pragma once

#include <juce_core/juce_core.h>

/**
 * Type of capture control value input.
 */
enum class ControlType
{
    DISCRETE,    // Explicit list of values (e.g., "ON, OFF" or "Low, Mid, High")
    CONTINUOUS   // Range syntax (e.g., "0-10:1" meaning 0 to 10 step 1)
};

/**
 * A single capture control definition for the matrix.
 * 
 * A control represents one parameter of the hardware being captured
 * (e.g., "Gain", "Tone", "Mix"). It has a name, type, and a set of
 * values that the parameter can take.
 */
struct CaptureControl
{
    juce::String id;             // Unique identifier (UUID)
    juce::String name;           // Display name (e.g., "Gain", "Tone")
    ControlType type = ControlType::DISCRETE;
    juce::StringArray values;    // Parsed list of values

    /**
     * Parse values from input string based on type.
     * 
     * For DISCRETE: comma-separated values (e.g., "Low, Mid, High")
     * For CONTINUOUS: range syntax "start-end:step" (e.g., "0-10:1")
     * 
     * @param input The input string to parse
     * @return true if parsing succeeded and at least one value was generated
     */
    bool parseValues(const juce::String& input);

    /**
     * Get the number of values for this control.
     */
    int getValueCount() const { return values.size(); }
};

/**
 * One enumerated cell of the capture matrix (a specific combination of
 * control values). Not a capture item and not the roundtrip.
 */
struct MatrixCombination
{
    juce::String key;                     // canonical key (see makeCombinationKey)
    juce::StringPairArray controlValues;  // control name -> value
};

/**
 * Manager for capture controls (the matrix definition).
 *
 * This class manages the collection of controls that define what
 * parameter combinations will be captured.
 */
class CaptureControlManager
{
public:
    CaptureControlManager() = default;

    /**
     * Add a new capture control.
     * 
     * @param name Display name for the control
     * @param type DISCRETE or CONTINUOUS
     * @param valuesInput Input string to parse for values
     * @return The ID of the newly created control
     */
    juce::String addControl(const juce::String& name, ControlType type, const juce::String& valuesInput);

    /**
     * Remove a capture control by ID.
     * 
     * @param id The control ID to remove
     * @return true if found and removed
     */
    bool removeControl(const juce::String& id);

    /**
     * Update an existing capture control.
     * 
     * @param id The control ID to update
     * @param name New display name
     * @param type New control type
     * @param valuesInput New values input string
     * @return true if found and updated
     */
    bool updateControl(const juce::String& id, const juce::String& name, 
                       ControlType type, const juce::String& valuesInput);

    /**
     * Get all controls.
     */
    const juce::Array<CaptureControl>& getControls() const { return controls; }

    /**
     * Get mutable access to controls (for serialization).
     */
    juce::Array<CaptureControl>& getControlsRef() { return controls; }

    /**
     * Clear all controls.
     */
    void clear() { controls.clear(); excludedKeys.clear(); }

    /**
     * Get the total number of capture combinations (product of all control value counts).
     */
    int getTotalCaptureCount() const;

    /** Total number of combinations (== getTotalCaptureCount). */
    int getTotalCombinationCount() const { return getTotalCaptureCount(); }

    /**
     * Canonical key for a combination: "name=value" for each control, in the
     * controls' defined order, joined by the unit-separator char (31).
     */
    juce::String makeCombinationKey(const juce::StringPairArray& controlValues) const;

    /**
     * Enumerate the full cartesian product (no roundtrip). Empty when there are
     * no controls or the product exceeds 100000.
     */
    juce::Array<MatrixCombination> getCombinations() const;

    /** True if this combination key is currently excluded. */
    bool isExcluded(const juce::String& key) const { return excludedKeys.contains(key); }

    /** Add/remove a key from the excluded set. */
    void setExcluded(const juce::String& key, bool excluded);

    /** Number of combinations NOT excluded. */
    int getIncludedCount() const;

    /**
     * Drop excluded keys that no longer match any current combination (after a
     * control edit). Returns the number removed.
     */
    int pruneStrandedKeys();

    const juce::StringArray& getExcludedKeys() const { return excludedKeys; }
    void setExcludedKeys(const juce::StringArray& keys) { excludedKeys = keys; }

    /**
     * Convert controls to juce::var for JSON serialization.
     */
    juce::var toVar() const;

private:
    juce::Array<CaptureControl> controls;
    juce::StringArray excludedKeys;   // canonical keys currently excluded

    /** Generate a unique ID for a new control */
    static juce::String generateId();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CaptureControlManager)
};
