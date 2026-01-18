#include "LevelMeter.h"

namespace playfultones
{

void LevelMeter::setSampleRate(double sampleRate)
{
    if (sampleRate <= 0.0)
        return;
    
    // Calculate coefficient for exponential moving average
    // Time constant tau = integrationTime, coefficient = exp(-1 / (tau * sampleRate))
    // For 300ms at 44100Hz: exp(-1 / (0.3 * 44100)) = exp(-1/13230) ≈ 0.999924
    double tau = kDefaultIntegrationTimeMs / 1000.0;  // Convert to seconds
    rmsCoefficient = std::exp(-1.0 / (tau * sampleRate));
}

void LevelMeter::process(const float* data, int numSamples)
{
    if (data == nullptr || numSamples == 0)
        return;

    float peakVal = 0.0f;
    
    // Process each sample through EMA for RMS, track peak
    for (int i = 0; i < numSamples; ++i)
    {
        const float sample = data[i];
        const float absSample = std::fabs(sample);
        const double squared = static_cast<double>(sample) * static_cast<double>(sample);
        
        // Exponential moving average of squared samples
        rmsSquaredAccum = rmsCoefficient * rmsSquaredAccum + (1.0 - rmsCoefficient) * squared;
        
        if (absSample > peakVal)
            peakVal = absSample;
    }
    
    // RMS is sqrt of the averaged squared value
    float rmsVal = static_cast<float>(std::sqrt(rmsSquaredAccum));

    rms.store(rmsVal, std::memory_order_relaxed);
    peak.store(peakVal, std::memory_order_relaxed);
    
    // Update RMS hold if current RMS exceeds stored value
    float currentRmsHold = rmsHold.load(std::memory_order_relaxed);
    if (rmsVal > currentRmsHold)
        rmsHold.store(rmsVal, std::memory_order_relaxed);
    
    // Update peak hold if current peak exceeds stored value
    float currentPeakHold = peakHold.load(std::memory_order_relaxed);
    if (peakVal > currentPeakHold)
        peakHold.store(peakVal, std::memory_order_relaxed);
}

MeterValues LevelMeter::getValues() const
{
    MeterValues values;
    values.rmsDb = linearToDb(rms.load(std::memory_order_relaxed));
    values.peakDb = linearToDb(peak.load(std::memory_order_relaxed));
    values.rmsHoldDb = linearToDb(rmsHold.load(std::memory_order_relaxed));
    values.peakHoldDb = linearToDb(peakHold.load(std::memory_order_relaxed));
    return values;
}

void LevelMeter::reset()
{
    rms.store(0.0f, std::memory_order_relaxed);
    peak.store(0.0f, std::memory_order_relaxed);
    rmsHold.store(0.0f, std::memory_order_relaxed);
    peakHold.store(0.0f, std::memory_order_relaxed);
    rmsSquaredAccum = 0.0;
}

void LevelMeter::resetPeakHold()
{
    rmsHold.store(0.0f, std::memory_order_relaxed);
    peakHold.store(0.0f, std::memory_order_relaxed);
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
