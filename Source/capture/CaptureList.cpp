#include "CaptureList.h"

//==============================================================================
// CaptureListManager

juce::String CaptureListManager::generateItemId()
{
    auto timestamp = juce::Time::getMillisecondCounter();
    auto random = juce::Random::getSystemRandom().nextInt();
    return "cap_" + juce::String::toHexString(timestamp) + juce::String::toHexString(random);
}

void CaptureListManager::generate(const CaptureControlManager& controlManager)
{
    items.clear();

    // Always add the roundtrip entry first (unit bypassed, no control values)
    CaptureItem roundtripItem;
    roundtripItem.id = generateItemId();
    roundtripItem.index = 1;
    roundtripItem.status = CaptureStatus::PENDING;
    roundtripItem.isRoundtrip = true;
    // No control values for roundtrip - unit will be bypassed
    items.add(roundtripItem);

    const auto& controls = controlManager.getControls();
    if (controls.isEmpty())
        return;

    // Calculate total combinations (safety check)
    int totalCount = controlManager.getTotalCaptureCount();
    if (totalCount == 0 || totalCount > 100000)
        return;

    // Build list of control names and their values for easier iteration
    juce::StringArray controlNames;
    juce::Array<juce::StringArray> controlValueLists;

    for (const auto& ctrl : controls)
    {
        controlNames.add(ctrl.name);
        controlValueLists.add(ctrl.values);
    }

    // Generate cartesian product using iterative approach
    // Start with indices all at 0
    juce::Array<int> indices;
    indices.resize(controls.size());
    for (int i = 0; i < indices.size(); ++i)
        indices.set(i, 0);

    int captureIndex = 2; // Start at 2 because roundtrip is at index 1

    while (true)
    {
        // Create a capture item with current combination
        CaptureItem item;
        item.id = generateItemId();
        item.index = captureIndex++;
        item.status = CaptureStatus::PENDING;
        item.isRoundtrip = false;

        // Set control values based on current indices
        for (int i = 0; i < controlNames.size(); ++i)
        {
            const auto& values = controlValueLists[i];
            item.controlValues.set(controlNames[i], values[indices[i]]);
        }

        items.add(item);

        // Increment indices (like a multi-digit counter)
        // Start from last control and propagate carry
        int position = indices.size() - 1;
        while (position >= 0)
        {
            indices.set(position, indices[position] + 1);

            if (indices[position] < controlValueLists[position].size())
            {
                // No overflow, we're done incrementing
                break;
            }

            // Overflow: reset this position and carry to next
            indices.set(position, 0);
            position--;
        }

        // If we've overflowed all positions, we're done
        if (position < 0)
            break;
    }
}

CaptureItem* CaptureListManager::findById(const juce::String& id)
{
    for (auto& item : items)
    {
        if (item.id == id)
            return &item;
    }
    return nullptr;
}

bool CaptureListManager::setStatus(const juce::String& id, CaptureStatus status)
{
    for (auto& item : items)
    {
        if (item.id == id)
        {
            item.status = status;
            return true;
        }
    }
    return false;
}

bool CaptureListManager::addOutputPath(const juce::String& id, const juce::String& path)
{
    for (auto& item : items)
    {
        if (item.id == id)
        {
            item.outputFilePaths.add(path);
            return true;
        }
    }
    return false;
}

bool CaptureListManager::clearOutputPaths(const juce::String& id)
{
    for (auto& item : items)
    {
        if (item.id == id)
        {
            item.outputFilePaths.clear();
            return true;
        }
    }
    return false;
}

juce::var CaptureListManager::toVar() const
{
    juce::Array<juce::var> result;

    for (const auto& item : items)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("id", item.id);
        obj->setProperty("index", item.index);
        obj->setProperty("status", statusToString(item.status));
        obj->setProperty("isRoundtrip", item.isRoundtrip);
        
        // Serialize output file paths as array
        juce::Array<juce::var> pathsArray;
        for (const auto& path : item.outputFilePaths)
            pathsArray.add(juce::var(path));
        obj->setProperty("outputFilePaths", juce::var(pathsArray));

        // Convert control values to object
        auto* valuesObj = new juce::DynamicObject();
        for (int i = 0; i < item.controlValues.size(); ++i)
        {
            auto key = item.controlValues.getAllKeys()[i];
            auto value = item.controlValues.getAllValues()[i];
            valuesObj->setProperty(juce::Identifier(key), value);
        }
        obj->setProperty("controlValues", juce::var(valuesObj));

        result.add(juce::var(obj));
    }

    return juce::var(result);
}

juce::String CaptureListManager::statusToString(CaptureStatus status)
{
    switch (status)
    {
        case CaptureStatus::PENDING: return "pending";
        case CaptureStatus::COMPLETE: return "complete";
        case CaptureStatus::FAILED: return "failed";
    }
    return "pending";
}

CaptureStatus CaptureListManager::stringToStatus(const juce::String& str)
{
    auto lower = str.toLowerCase();
    if (lower == "pending")
        return CaptureStatus::PENDING;
    if (lower == "complete" || lower == "done")
        return CaptureStatus::COMPLETE;
    if (lower == "failed")
        return CaptureStatus::FAILED;
    return CaptureStatus::PENDING;
}
