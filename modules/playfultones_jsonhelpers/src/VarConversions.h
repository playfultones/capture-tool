#pragma once

namespace playfultones
{
namespace json
{

//==============================================================================
// Array to var conversions

/**
 * Convert a StringArray to a juce::var array.
 */
juce::var toVar(const juce::StringArray& strings);

/**
 * Convert an Array<int> to a juce::var array.
 */
juce::var toVar(const juce::Array<int>& ints);

/**
 * Convert an Array<float> to a juce::var array.
 */
juce::var toVar(const juce::Array<float>& floats);

/**
 * Convert an Array<double> to a juce::var array.
 */
juce::var toVar(const juce::Array<double>& doubles);

/**
 * Convert a StringPairArray to a juce::var object.
 */
juce::var toVar(const juce::StringPairArray& pairs);

//==============================================================================
// var to array conversions

/**
 * Convert a juce::var array to StringArray.
 */
juce::StringArray toStringArray(const juce::var& v);

/**
 * Convert a juce::var array to Array<int>.
 */
juce::Array<int> toIntArray(const juce::var& v);

/**
 * Convert a juce::var array to Array<float>.
 */
juce::Array<float> toFloatArray(const juce::var& v);

//==============================================================================
// Timestamp helpers

/**
 * Get current timestamp in ISO 8601 format.
 * Example: "2024-01-15T14:30:00Z"
 */
juce::String isoTimestamp();

//==============================================================================
// ObjectBuilder - fluent API for building DynamicObjects

/**
 * Helper class for building juce::var objects with a fluent API.
 * 
 * Usage:
 *   auto obj = ObjectBuilder()
 *       .set("name", "John")
 *       .set("age", 30)
 *       .set("active", true)
 *       .build();
 */
class ObjectBuilder
{
public:
    ObjectBuilder();

    /**
     * Set a property on the object.
     * @return Reference to this builder for chaining
     */
    ObjectBuilder& set(const juce::Identifier& key, const juce::var& value);

    /**
     * Set a property using string key.
     * @return Reference to this builder for chaining
     */
    ObjectBuilder& set(const juce::String& key, const juce::var& value);

    /**
     * Build and return the final juce::var object.
     * The builder can be reused after calling build().
     */
    juce::var build();

    /**
     * Create a new DynamicObject pointer (caller takes ownership).
     * Useful when you need to add the object to another object.
     */
    juce::DynamicObject* createObject();

private:
    juce::ReferenceCountedObjectPtr<juce::DynamicObject> object;
};

//==============================================================================
// Convenience functions

/**
 * Create a new juce::var object with a single property.
 */
juce::var makeObject(const juce::Identifier& key, const juce::var& value);

/**
 * Create a new juce::var object with two properties.
 */
juce::var makeObject(const juce::Identifier& key1, const juce::var& value1,
                     const juce::Identifier& key2, const juce::var& value2);

} // namespace json
} // namespace playfultones
