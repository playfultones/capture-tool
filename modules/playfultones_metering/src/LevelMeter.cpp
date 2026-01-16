#include "LevelMeter.h"

namespace playfultones
{

void LevelMeter::process(const float* data, int numSamples)
{
    if (data == nullptr || numSamples == 0)
        return;

    float rmsVal, peakVal;
    calculateLevels(data, numSamples, rmsVal, peakVal);

    rms.store(rmsVal, std::memory_order_relaxed);
    peak.store(peakVal, std::memory_order_relaxed);
}

MeterValues LevelMeter::getValues() const
{
    MeterValues values;
    values.rmsDb = linearToDb(rms.load(std::memory_order_relaxed));
    values.peakDb = linearToDb(peak.load(std::memory_order_relaxed));
    return values;
}

void LevelMeter::reset()
{
    rms.store(0.0f, std::memory_order_relaxed);
    peak.store(0.0f, std::memory_order_relaxed);
}

float LevelMeter::linearToDb(float linear)
{
    if (linear <= 0.0f)
        return -100.0f;

    return 20.0f * std::log10(linear);
}

float LevelMeter::dbToLinear(float db)
{
    if (db <= -100.0f)
        return 0.0f;

    return std::pow(10.0f, db / 20.0f);
}

void LevelMeter::calculateLevels(const float* data, int numSamples, 
                                  float& outRms, float& outPeak)
{
    if (data == nullptr || numSamples == 0)
    {
        outRms = 0.0f;
        outPeak = 0.0f;
        return;
    }

    float sumSquares = 0.0f;
    float peakVal = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float sample = data[i];
        const float absSample = std::fabs(sample);
        sumSquares += sample * sample;

        if (absSample > peakVal)
            peakVal = absSample;
    }

    outRms = std::sqrt(sumSquares / static_cast<float>(numSamples));
    outPeak = peakVal;
}

} // namespace playfultones
