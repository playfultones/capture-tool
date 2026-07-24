#include "CaptureControl.h"
#include <playfultones_jsonhelpers/playfultones_jsonhelpers.h>
#include <cmath>

//==============================================================================
// CaptureControl

bool CaptureControl::parseValues(const juce::String& input)
{
    values.clear();

    if (input.trim().isEmpty())
        return false;

    if (type == ControlType::DISCRETE)
    {
        // Parse comma-separated values
        juce::StringArray parsed;
        parsed.addTokens(input, ",", "\"");

        for (auto& val : parsed)
        {
            auto trimmed = val.trim();
            if (trimmed.isNotEmpty())
                values.add(trimmed);
        }

        return values.size() > 0;
    }
    else // CONTINUOUS
    {
        // Parse range syntax: "start-end:step" or "start-end" (default step 1)
        // Examples: "0-10:1", "0-100:10", "0.0-1.0:0.1"

        auto colonIndex = input.lastIndexOfChar(':');
        juce::String rangeStr = input;
        float step = 1.0f;

        if (colonIndex > 0)
        {
            step = input.substring(colonIndex + 1).getFloatValue();
            rangeStr = input.substring(0, colonIndex);
            if (step <= 0.0f)
                step = 1.0f;
        }

        auto dashIndex = rangeStr.indexOf("-");
        // Handle negative start values - find dash that's not at position 0
        if (dashIndex == 0)
            dashIndex = rangeStr.substring(1).indexOf("-") + 1;

        if (dashIndex < 0)
            return false;

        float start = rangeStr.substring(0, dashIndex).getFloatValue();
        float end = rangeStr.substring(dashIndex + 1).getFloatValue();

        if (start > end)
            std::swap(start, end);

        // Generate values
        for (float val = start; val <= end + (step * 0.001f); val += step)
        {
            // Format nicely: use integer if whole number, otherwise use decimal
            if (std::abs(val - std::round(val)) < 0.001f)
                values.add(juce::String(static_cast<int>(std::round(val))));
            else
                values.add(juce::String(val, 2));

            // Safety limit to prevent infinite loops
            if (values.size() > 1000)
                break;
        }

        return values.size() > 0;
    }
}

//==============================================================================
// CaptureControlManager

juce::String CaptureControlManager::generateId()
{
    auto timestamp = juce::Time::getMillisecondCounter();
    auto random = juce::Random::getSystemRandom().nextInt();
    return juce::String::toHexString(timestamp) + juce::String::toHexString(random);
}

juce::String CaptureControlManager::addControl(const juce::String& name, ControlType type, 
                                                const juce::String& valuesInput)
{
    CaptureControl ctrl;
    ctrl.id = generateId();
    ctrl.name = name.trim().isEmpty() ? "Control " + juce::String(controls.size() + 1) : name.trim();
    ctrl.type = type;

    if (!ctrl.parseValues(valuesInput))
    {
        // Default to a single value if parsing fails
        ctrl.values.add("0");
    }

    controls.add(ctrl);
    return ctrl.id;
}

bool CaptureControlManager::removeControl(const juce::String& id)
{
    for (int i = 0; i < controls.size(); ++i)
    {
        if (controls[i].id == id)
        {
            controls.remove(i);
            return true;
        }
    }
    return false;
}

bool CaptureControlManager::updateControl(const juce::String& id, const juce::String& name,
                                           ControlType type, const juce::String& valuesInput)
{
    for (auto& ctrl : controls)
    {
        if (ctrl.id == id)
        {
            ctrl.name = name.trim().isEmpty() ? ctrl.name : name.trim();
            ctrl.type = type;
            ctrl.parseValues(valuesInput);
            return true;
        }
    }
    return false;
}

int CaptureControlManager::getTotalCaptureCount() const
{
    if (controls.isEmpty())
        return 0;

    int total = 1;
    for (const auto& ctrl : controls)
    {
        total *= ctrl.getValueCount();

        // Safety limit
        if (total > 100000)
            return 100000;
    }

    return total;
}

juce::var CaptureControlManager::toVar() const
{
    juce::Array<juce::var> result;

    for (const auto& ctrl : controls)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("id", ctrl.id);
        obj->setProperty("name", ctrl.name);
        obj->setProperty("type", ctrl.type == ControlType::DISCRETE ? "discrete" : "continuous");
        obj->setProperty("values", playfultones::json::toVar(ctrl.values));
        obj->setProperty("valueCount", ctrl.getValueCount());
        result.add(juce::var(obj));
    }

    return juce::var(result);
}

juce::String CaptureControlManager::makeCombinationKey(const juce::StringPairArray& controlValues) const
{
    static const juce::String sep = juce::String::charToString(static_cast<juce::juce_wchar>(31));

    juce::String key;
    for (int i = 0; i < controls.size(); ++i)
    {
        if (i > 0)
            key << sep;
        key << controls[i].name << "=" << controlValues[controls[i].name];
    }
    return key;
}

juce::Array<MatrixCombination> CaptureControlManager::getCombinations() const
{
    juce::Array<MatrixCombination> result;

    if (controls.isEmpty())
        return result;

    // Exact product with 64-bit math and early bail-out. getTotalCombinationCount()
    // CLAMPS at 100000 and returns exactly 100000 when the product overflows it, so
    // it cannot be used as the guard here — recompute exactly and refuse to
    // enumerate anything too large to handle interactively (this runs on every edit).
    juce::int64 total = 1;
    for (const auto& ctrl : controls)
    {
        total *= (juce::int64) ctrl.getValueCount();
        if (total > 100000)
            return result;   // too large to enumerate/exclude in the UI
    }
    if (total == 0)
        return result;

    juce::Array<int> indices;
    indices.resize(controls.size());
    for (int i = 0; i < indices.size(); ++i)
        indices.set(i, 0);

    while (true)
    {
        MatrixCombination combo;
        for (int i = 0; i < controls.size(); ++i)
            combo.controlValues.set(controls[i].name, controls[i].values[indices[i]]);
        combo.key = makeCombinationKey(combo.controlValues);
        result.add(combo);

        int position = indices.size() - 1;
        while (position >= 0)
        {
            indices.set(position, indices[position] + 1);
            if (indices[position] < controls[position].values.size())
                break;
            indices.set(position, 0);
            position--;
        }
        if (position < 0)
            break;
    }

    return result;
}

void CaptureControlManager::setExcluded(const juce::String& key, bool excluded)
{
    if (excluded)
    {
        if (!excludedKeys.contains(key))
            excludedKeys.add(key);
    }
    else
    {
        excludedKeys.removeString(key);
    }
}

int CaptureControlManager::getIncludedCount() const
{
    int included = 0;
    for (const auto& combo : getCombinations())
        if (!isExcluded(combo.key))
            ++included;
    return included;
}

int CaptureControlManager::pruneStrandedKeys()
{
    juce::StringArray valid;
    for (const auto& combo : getCombinations())
        valid.add(combo.key);

    int removed = 0;
    for (int i = excludedKeys.size(); --i >= 0;)
    {
        if (!valid.contains(excludedKeys[i]))
        {
            excludedKeys.remove(i);
            ++removed;
        }
    }
    return removed;
}
