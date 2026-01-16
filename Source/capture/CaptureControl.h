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
    void clear() { controls.clear(); }

    /**
     * Get the total number of capture combinations (product of all control value counts).
     */
    int getTotalCaptureCount() const;

    /**
     * Convert controls to juce::var for JSON serialization.
     */
    juce::var toVar() const;

private:
    juce::Array<CaptureControl> controls;

    /** Generate a unique ID for a new control */
    static juce::String generateId();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CaptureControlManager)
};
