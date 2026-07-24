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

    // Always add the roundtrip entry first (unit bypassed, no control values).
    CaptureItem roundtripItem;
    roundtripItem.id = generateItemId();
    roundtripItem.index = 1;
    roundtripItem.status = CaptureStatus::PENDING;
    roundtripItem.isRoundtrip = true;
    items.add(roundtripItem);

    // Emit one item per included combination, in enumeration order.
    int captureIndex = 2; // roundtrip is index 1
    for (const auto& combo : controlManager.getCombinations())
    {
        if (controlManager.isExcluded(combo.key))
            continue;

        CaptureItem item;
        item.id = generateItemId();
        item.index = captureIndex++;
        item.status = CaptureStatus::PENDING;
        item.isRoundtrip = false;
        item.controlValues = combo.controlValues;
        items.add(item);
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
