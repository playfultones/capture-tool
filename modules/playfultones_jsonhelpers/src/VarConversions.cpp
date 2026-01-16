#include "VarConversions.h"

namespace playfultones
{
namespace json
{

//==============================================================================
// Array to var conversions

juce::var toVar(const juce::StringArray& strings)
{
    juce::Array<juce::var> result;
    result.ensureStorageAllocated(strings.size());

    for (const auto& str : strings)
        result.add(juce::var(str));

    return juce::var(result);
}

juce::var toVar(const juce::Array<int>& ints)
{
    juce::Array<juce::var> result;
    result.ensureStorageAllocated(ints.size());

    for (const auto& val : ints)
        result.add(juce::var(val));

    return juce::var(result);
}

juce::var toVar(const juce::Array<float>& floats)
{
    juce::Array<juce::var> result;
    result.ensureStorageAllocated(floats.size());

    for (const auto& val : floats)
        result.add(juce::var(static_cast<double>(val)));

    return juce::var(result);
}

juce::var toVar(const juce::Array<double>& doubles)
{
    juce::Array<juce::var> result;
    result.ensureStorageAllocated(doubles.size());

    for (const auto& val : doubles)
        result.add(juce::var(val));

    return juce::var(result);
}

juce::var toVar(const juce::StringPairArray& pairs)
{
    auto* obj = new juce::DynamicObject();

    for (int i = 0; i < pairs.size(); ++i)
    {
        auto key = pairs.getAllKeys()[i];
        auto value = pairs.getAllValues()[i];
        obj->setProperty(juce::Identifier(key), value);
    }

    return juce::var(obj);
}

//==============================================================================
// var to array conversions

juce::StringArray toStringArray(const juce::var& v)
{
    juce::StringArray result;

    if (auto* arr = v.getArray())
    {
        result.ensureStorageAllocated(arr->size());
        for (const auto& item : *arr)
            result.add(item.toString());
    }

    return result;
}

juce::Array<int> toIntArray(const juce::var& v)
{
    juce::Array<int> result;

    if (auto* arr = v.getArray())
    {
        result.ensureStorageAllocated(arr->size());
        for (const auto& item : *arr)
            result.add(static_cast<int>(item));
    }

    return result;
}

juce::Array<float> toFloatArray(const juce::var& v)
{
    juce::Array<float> result;

    if (auto* arr = v.getArray())
    {
        result.ensureStorageAllocated(arr->size());
        for (const auto& item : *arr)
            result.add(static_cast<float>(item));
    }

    return result;
}

//==============================================================================
// Timestamp helpers

juce::String isoTimestamp()
{
    return juce::Time::getCurrentTime().toISO8601(true);
}

//==============================================================================
// ObjectBuilder

ObjectBuilder::ObjectBuilder()
    : object(new juce::DynamicObject())
{
}

ObjectBuilder& ObjectBuilder::set(const juce::Identifier& key, const juce::var& value)
{
    object->setProperty(key, value);
    return *this;
}

ObjectBuilder& ObjectBuilder::set(const juce::String& key, const juce::var& value)
{
    object->setProperty(juce::Identifier(key), value);
    return *this;
}

juce::var ObjectBuilder::build()
{
    auto result = juce::var(object.get());
    object = new juce::DynamicObject(); // Reset for reuse
    return result;
}

juce::DynamicObject* ObjectBuilder::createObject()
{
    auto* result = object.get();
    object = new juce::DynamicObject(); // Reset for reuse
    return result;
}

//==============================================================================
// Convenience functions

juce::var makeObject(const juce::Identifier& key, const juce::var& value)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty(key, value);
    return juce::var(obj);
}

juce::var makeObject(const juce::Identifier& key1, const juce::var& value1,
                     const juce::Identifier& key2, const juce::var& value2)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty(key1, value1);
    obj->setProperty(key2, value2);
    return juce::var(obj);
}

} // namespace json
} // namespace playfultones
