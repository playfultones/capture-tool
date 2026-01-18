#include "CaptureLog.h"

void CaptureLogManager::setOutputFolder(const juce::File& folder)
{
    outputFolder = folder;
    logFile = juce::File();
    initialized = false;
}

void CaptureLogManager::reset()
{
    logFile = juce::File();
    initialized = false;
}

juce::String CaptureLogManager::getCurrentTimestamp()
{
    return juce::Time::getCurrentTime().toISO8601(true);
}

juce::var CaptureLogManager::createEmptyLogStructure() const
{
    auto* sessionObj = new juce::DynamicObject();
    sessionObj->setProperty("created", getCurrentTimestamp());
    sessionObj->setProperty("referenceSignal", "");
    sessionObj->setProperty("sampleRate", 0);
    sessionObj->setProperty("tailMs", 0);
    
    auto* logObj = new juce::DynamicObject();
    logObj->setProperty("session", juce::var(sessionObj));
    logObj->setProperty("captures", juce::Array<juce::var>());
    
    return juce::var(logObj);
}

juce::var CaptureLogManager::readOrCreateLog() const
{
    if (!logFile.existsAsFile())
        return createEmptyLogStructure();
    
    // Read existing file
    auto jsonString = logFile.loadFileAsString();
    auto result = juce::JSON::parse(jsonString);
    
    if (result.isObject())
        return result;
    
    // Return empty structure if parsing failed
    return createEmptyLogStructure();
}

bool CaptureLogManager::writeLog(const juce::var& logData)
{
    if (!outputFolder.exists() || !outputFolder.isDirectory())
        return false;
    
    if (logFile.getFullPathName().isEmpty())
        logFile = outputFolder.getChildFile("capture_log.json");
    
    // Write JSON with pretty formatting
    auto jsonString = juce::JSON::toString(logData, true);
    return logFile.replaceWithText(jsonString);
}

void CaptureLogManager::initializeLog(const juce::String& referenceSignalPath,
                                      int sampleRate,
                                      int tailMs)
{
    if (!outputFolder.exists() || !outputFolder.isDirectory())
        return;
    
    logFile = outputFolder.getChildFile("capture_log.json");
    
    // Create session metadata
    auto* sessionObj = new juce::DynamicObject();
    sessionObj->setProperty("created", getCurrentTimestamp());
    
    // Reference signal info
    if (referenceSignalPath.isNotEmpty())
    {
        juce::File refFile(referenceSignalPath);
        sessionObj->setProperty("referenceSignal", refFile.getFileName());
    }
    else
    {
        sessionObj->setProperty("referenceSignal", "");
    }
    
    sessionObj->setProperty("sampleRate", sampleRate);
    sessionObj->setProperty("tailMs", tailMs);
    
    // Create the log structure
    auto* logObj = new juce::DynamicObject();
    logObj->setProperty("session", juce::var(sessionObj));
    logObj->setProperty("captures", juce::Array<juce::var>());
    
    writeLog(juce::var(logObj));
    initialized = true;
}

void CaptureLogManager::appendCapture(const CaptureItem& item,
                                      const juce::String& referenceSignalPath,
                                      int sampleRate,
                                      int tailMs,
                                      double durationSeconds,
                                      float peakLevelDb,
                                      float rmsLevelDb)
{
    // Initialize log on first capture
    if (!initialized)
    {
        initializeLog(referenceSignalPath, sampleRate, tailMs);
    }
    
    // Read existing log
    auto logData = readOrCreateLog();
    
    if (!logData.isObject())
        return;
    
    auto* logObj = logData.getDynamicObject();
    if (logObj == nullptr)
        return;
    
    // Get or create captures array
    auto capturesVar = logObj->getProperty("captures");
    juce::Array<juce::var>* captures = capturesVar.getArray();
    
    if (captures == nullptr)
    {
        // Create new array if it doesn't exist
        logObj->setProperty("captures", juce::Array<juce::var>());
        captures = logObj->getProperty("captures").getArray();
    }
    
    if (captures == nullptr)
        return;
    
    // Create capture entry
    auto* captureObj = new juce::DynamicObject();
    captureObj->setProperty("id", item.id);
    captureObj->setProperty("timestamp", getCurrentTimestamp());
    
    // Settings (control values)
    auto* settingsObj = new juce::DynamicObject();
    for (int i = 0; i < item.controlValues.size(); ++i)
    {
        auto key = item.controlValues.getAllKeys()[i];
        auto value = item.controlValues.getAllValues()[i];
        settingsObj->setProperty(juce::Identifier(key), value);
    }
    captureObj->setProperty("settings", juce::var(settingsObj));
    
    // Output file (just the filename, not full path)
    // Use the last output path in the array (most recent capture)
    juce::String outputPath = item.outputFilePaths.isEmpty() ? juce::String() : item.outputFilePaths[item.outputFilePaths.size() - 1];
    juce::File outputFile(outputPath);
    captureObj->setProperty("outputFile", outputFile.getFileName());
    
    // Duration in ms
    captureObj->setProperty("durationMs", static_cast<int>(durationSeconds * 1000.0));
    
    // Level info
    captureObj->setProperty("inputLevelPeak", static_cast<double>(peakLevelDb));
    captureObj->setProperty("inputLevelRms", static_cast<double>(rmsLevelDb));
    
    // Add to captures array
    captures->add(juce::var(captureObj));
    
    // Write updated log
    writeLog(logData);
}
