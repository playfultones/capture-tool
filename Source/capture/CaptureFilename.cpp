#include "CaptureFilename.h"

juce::String CaptureFilenameGenerator::formatSampleRateForFilename(int sampleRate)
{
    // Convert sample rate to compact format: 44100 -> "44k1", 48000 -> "48k", 96000 -> "96k"
    if (sampleRate % 1000 == 0)
    {
        return juce::String(sampleRate / 1000) + "k";
    }
    else if (sampleRate == 44100)
    {
        return "44k1";
    }
    else
    {
        // Fallback: use full number
        return juce::String(sampleRate);
    }
}

juce::String CaptureFilenameGenerator::sanitizeForFilename(const juce::String& str)
{
    return str.removeCharacters(" /\\:*?\"<>|");
}

juce::String CaptureFilenameGenerator::generateFilename(const CaptureItem& item,
                                                        const juce::Array<CaptureControl>& controls,
                                                        const juce::String& referenceSignalPath,
                                                        int sampleRate)
{
    juce::String filename;
    
    // 1. Reference signal name (without extension)
    juce::String signalName;
    if (referenceSignalPath.isNotEmpty())
    {
        auto refFile = juce::File(referenceSignalPath);
        signalName = refFile.getFileNameWithoutExtension();
    }
    else
    {
        signalName = "capture";
    }
    filename += signalName;
    
    // 2. Sample rate
    filename += "_" + formatSampleRateForFilename(sampleRate);
    
    // 3. For roundtrip entries, use "_roundtrip" instead of control values
    if (item.isRoundtrip)
    {
        filename += "_roundtrip";
    }
    else
    {
        // Control values (in order they appear in controls)
        // We iterate through controls to maintain consistent ordering
        for (const auto& ctrl : controls)
        {
            juce::String value = item.controlValues[ctrl.name];
            if (value.isNotEmpty())
            {
                // Sanitize control name and value for filename
                juce::String safeName = sanitizeForFilename(ctrl.name);
                juce::String safeValue = sanitizeForFilename(value);
                
                filename += "_" + safeName + "-" + safeValue;
            }
        }
    }
    
    // 4. Add .wav extension
    filename += ".wav";
    
    return filename;
}

juce::String CaptureFilenameGenerator::getExpectedOutputPath(const CaptureItem& item,
                                                             const juce::Array<CaptureControl>& controls,
                                                             const juce::String& referenceSignalPath,
                                                             int sampleRate,
                                                             const juce::File& outputFolder)
{
    if (!outputFolder.exists() || !outputFolder.isDirectory())
        return juce::String();
    
    juce::String filename = generateFilename(item, controls, referenceSignalPath, sampleRate);
    return outputFolder.getChildFile(filename).getFullPathName();
}
