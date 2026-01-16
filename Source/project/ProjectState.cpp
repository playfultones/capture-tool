#include "ProjectState.h"

namespace ProjectSerializer
{

juce::var serializeAudioSettings(const ProjectAudioSettings& settings)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("inputDevice", settings.inputDevice);
    obj->setProperty("inputChannel", settings.inputChannel);
    obj->setProperty("outputDevice", settings.outputDevice);
    obj->setProperty("outputChannel", settings.outputChannel);
    obj->setProperty("sampleRate", settings.sampleRate);
    return juce::var(obj);
}

ProjectAudioSettings deserializeAudioSettings(const juce::var& data)
{
    ProjectAudioSettings settings;
    
    if (auto* obj = data.getDynamicObject())
    {
        settings.inputDevice = obj->getProperty("inputDevice").toString();
        settings.inputChannel = static_cast<int>(obj->getProperty("inputChannel"));
        settings.outputDevice = obj->getProperty("outputDevice").toString();
        settings.outputChannel = static_cast<int>(obj->getProperty("outputChannel"));
        settings.sampleRate = static_cast<int>(obj->getProperty("sampleRate"));
    }
    
    return settings;
}

juce::var serializeCaptureSettings(const ProjectCaptureSettings& settings)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("tailMs", settings.tailMs);
    obj->setProperty("outputFolder", settings.outputFolderPath);
    return juce::var(obj);
}

ProjectCaptureSettings deserializeCaptureSettings(const juce::var& data)
{
    ProjectCaptureSettings settings;
    
    if (auto* obj = data.getDynamicObject())
    {
        settings.tailMs = static_cast<int>(obj->getProperty("tailMs"));
        settings.outputFolderPath = obj->getProperty("outputFolder").toString();
    }
    
    return settings;
}

juce::var serializeReferenceSignal(const ProjectReferenceSignal& signal)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("path", signal.path);
    obj->setProperty("durationMs", signal.durationMs);
    return juce::var(obj);
}

ProjectReferenceSignal deserializeReferenceSignal(const juce::var& data)
{
    ProjectReferenceSignal signal;
    
    if (auto* obj = data.getDynamicObject())
    {
        signal.path = obj->getProperty("path").toString();
        signal.durationMs = static_cast<int>(obj->getProperty("durationMs"));
    }
    
    return signal;
}

juce::var serializeControls(const juce::Array<CaptureControl>& controls)
{
    juce::Array<juce::var> controlsArray;
    
    for (const auto& ctrl : controls)
    {
        auto* ctrlObj = new juce::DynamicObject();
        ctrlObj->setProperty("name", ctrl.name);
        ctrlObj->setProperty("type", ctrl.type == ControlType::DISCRETE ? "discrete" : "continuous");
        ctrlObj->setProperty("values", stringArrayToVar(ctrl.values));
        controlsArray.add(juce::var(ctrlObj));
    }
    
    auto* matrixObj = new juce::DynamicObject();
    matrixObj->setProperty("controls", juce::var(controlsArray));
    return juce::var(matrixObj);
}

juce::Array<CaptureControl> deserializeControls(const juce::var& data,
                                                std::function<juce::String()> generateId)
{
    juce::Array<CaptureControl> controls;
    
    if (auto* matrix = data.getDynamicObject())
    {
        auto controlsVar = matrix->getProperty("controls");
        if (auto* controlsArray = controlsVar.getArray())
        {
            for (const auto& ctrlVar : *controlsArray)
            {
                if (auto* ctrlObj = ctrlVar.getDynamicObject())
                {
                    CaptureControl ctrl;
                    ctrl.id = generateId();
                    ctrl.name = ctrlObj->getProperty("name").toString();
                    
                    auto typeStr = ctrlObj->getProperty("type").toString();
                    ctrl.type = (typeStr == "continuous") ? ControlType::CONTINUOUS : ControlType::DISCRETE;
                    
                    // Load values array
                    auto valuesVar = ctrlObj->getProperty("values");
                    if (auto* valuesArray = valuesVar.getArray())
                    {
                        for (const auto& val : *valuesArray)
                        {
                            ctrl.values.add(val.toString());
                        }
                    }
                    
                    controls.add(ctrl);
                }
            }
        }
    }
    
    return controls;
}

juce::var serializeCaptureList(const juce::Array<CaptureItem>& items)
{
    juce::Array<juce::var> capturesArray;
    
    for (const auto& item : items)
    {
        auto* captureObj = new juce::DynamicObject();
        captureObj->setProperty("id", item.id);
        
        // Convert settings to object
        auto* settingsObj = new juce::DynamicObject();
        for (int i = 0; i < item.controlValues.size(); ++i)
        {
            auto key = item.controlValues.getAllKeys()[i];
            auto value = item.controlValues.getAllValues()[i];
            settingsObj->setProperty(juce::Identifier(key), value);
        }
        captureObj->setProperty("settings", juce::var(settingsObj));
        
        // Status string
        captureObj->setProperty("status", CaptureListManager::statusToString(item.status));
        
        // Output file (null/empty if not captured yet)
        if (item.outputFilePath.isNotEmpty())
            captureObj->setProperty("outputFile", item.outputFilePath);
        else
            captureObj->setProperty("outputFile", juce::var());
        
        capturesArray.add(juce::var(captureObj));
    }
    
    return juce::var(capturesArray);
}

juce::Array<CaptureItem> deserializeCaptureList(const juce::var& data,
                                                std::function<juce::String()> generateId)
{
    juce::Array<CaptureItem> items;
    
    if (auto* capturesArray = data.getArray())
    {
        int captureIndex = 1;
        
        for (const auto& captureVar : *capturesArray)
        {
            if (auto* captureObj = captureVar.getDynamicObject())
            {
                CaptureItem item;
                item.id = captureObj->getProperty("id").toString();
                if (item.id.isEmpty())
                    item.id = generateId();
                
                item.index = captureIndex++;
                
                // Parse status
                auto statusStr = captureObj->getProperty("status").toString();
                item.status = CaptureListManager::stringToStatus(statusStr);
                
                // Parse output file path
                auto outputFileVar = captureObj->getProperty("outputFile");
                if (outputFileVar.isString())
                    item.outputFilePath = outputFileVar.toString();
                
                // Parse control values (settings)
                auto settingsVar = captureObj->getProperty("settings");
                if (auto* settingsObj = settingsVar.getDynamicObject())
                {
                    auto& props = settingsObj->getProperties();
                    for (int i = 0; i < props.size(); ++i)
                    {
                        auto key = props.getName(i).toString();
                        auto value = props.getValueAt(i).toString();
                        item.controlValues.set(key, value);
                    }
                }
                
                items.add(item);
            }
        }
    }
    
    return items;
}

} // namespace ProjectSerializer
