/**
 * EngineHarness - CLI verification harness for the Reference Capture Tool audio engine.
 *
 * Purpose: verify the output->input roundtrip and recording flow against real
 * hardware, without the WebView/GUI. Built to diagnose "no input signal during
 * calibration".
 *
 * Rig assumption (overridable via flags): RME Fireface, test signal out physical
 * output 3 (0-based index 2) -> reamp -> physical input 10 (0-based index 9).
 *
 * Modes:
 *   list                          Enumerate devices and channel counts.
 *   probe  [opts]                 Raw diagnostic: open device directly, play a
 *                                 1kHz tone on --out, print RMS of EVERY input
 *                                 channel. Ground truth of the loop + JUCE's
 *                                 channel index -> physical mapping.
 *   engine [opts]                 Exercise the real AudioEngine exactly as the
 *                                 app does (initialize, setOutputDevice,
 *                                 setInputDevice, test tone), poll the input
 *                                 meter, then record to a WAV and report its level.
 *
 * Common opts:
 *   --device <substr>   Device name substring to match (default "Fireface")
 *   --out <idx>         Output channel index, 0-based (default 2 = output 3)
 *   --in <idx>          Input channel index, 0-based (default 9 = input 10)
 *   --secs <f>          Duration of the measurement (default 1.5)
 */

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include "AudioEngine.h"

#include <cmath>

namespace
{
constexpr double kToneFreq = 1000.0;
constexpr double kToneAmp = 0.125892541; // -18 dBFS

juce::String pad (const juce::String& s, int width)
{
    juce::String r = s;
    while (r.length() < width) r += " ";
    return r;
}

juce::String dbfs (float linear)
{
    if (linear <= 1.0e-7f) return "  -inf ";
    float db = 20.0f * std::log10 (linear);
    return juce::String (db, 1) + " dB";
}

// Find a device name containing the substring (case-insensitive), searching a list.
juce::String matchDevice (const juce::StringArray& names, const juce::String& substr)
{
    for (const auto& n : names)
        if (n.containsIgnoreCase (substr))
            return n;
    return {};
}

//==============================================================================
// Raw probe callback: play a tone on one output channel, measure RMS of ALL
// input channels. Establishes hardware/loopback ground truth independent of
// AudioEngine.
class ProbeCallback : public juce::AudioIODeviceCallback
{
public:
    ProbeCallback (int outIndex) : outCh (outIndex) {}

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override
    {
        for (int ch = 0; ch < numOutputChannels; ++ch)
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);

        // Lazily size the accumulators to the actual channel count.
        if ((int) sumSquares.size() != numInputChannels)
        {
            sumSquares.assign ((size_t) numInputChannels, 0.0);
            channelNull.assign ((size_t) numInputChannels, 0);
        }
        capturedInputChans = numInputChannels;

        const double inc = 2.0 * juce::MathConstants<double>::pi * kToneFreq / sampleRate;

        if (outCh >= 0 && outCh < numOutputChannels && outputChannelData[outCh] != nullptr)
        {
            float* out = outputChannelData[outCh];
            for (int i = 0; i < numSamples; ++i)
            {
                out[i] = (float) (std::sin (phase) * kToneAmp);
                phase += inc;
                if (phase >= 2.0 * juce::MathConstants<double>::pi)
                    phase -= 2.0 * juce::MathConstants<double>::pi;
            }
        }

        for (int ch = 0; ch < numInputChannels; ++ch)
        {
            if (inputChannelData[ch] == nullptr) { channelNull[(size_t) ch] = 1; continue; }
            double s = 0.0;
            for (int i = 0; i < numSamples; ++i)
                s += (double) inputChannelData[ch][i] * inputChannelData[ch][i];
            sumSquares[(size_t) ch] += s;
        }
        totalSamples += numSamples;
    }

    void audioDeviceAboutToStart (juce::AudioIODevice* d) override
    {
        sampleRate = d->getCurrentSampleRate();
    }
    void audioDeviceStopped() override {}

    double sampleRate = 48000.0;
    int outCh;
    double phase = 0.0;
    std::vector<double> sumSquares;
    std::vector<int> channelNull;
    juce::int64 totalSamples = 0;
    int capturedInputChans = 0;
};

struct Opts
{
    juce::String device = "Fireface";
    int out = 2;
    int in = 9;
    double secs = 1.5;
};

Opts parseOpts (const juce::StringArray& args)
{
    Opts o;
    for (int i = 0; i < args.size() - 1; ++i)
    {
        if (args[i] == "--device") o.device = args[i + 1];
        else if (args[i] == "--out") o.out = args[i + 1].getIntValue();
        else if (args[i] == "--in") o.in = args[i + 1].getIntValue();
        else if (args[i] == "--secs") o.secs = args[i + 1].getDoubleValue();
    }
    return o;
}

//==============================================================================
int doList()
{
    AudioEngine engine;
    if (! engine.initialize())
    {
        std::cout << "ERROR: audio init failed (mic permission?)\n";
        return 1;
    }
    auto ins = engine.getInputDeviceNames();
    auto outs = engine.getOutputDeviceNames();

    std::cout << "\n=== INPUT DEVICES ===\n";
    for (const auto& n : ins)
        std::cout << "  " << pad (n, 40) << " channels: " << engine.getInputChannelCount (n) << "\n";
    std::cout << "\n=== OUTPUT DEVICES ===\n";
    for (const auto& n : outs)
        std::cout << "  " << pad (n, 40) << " channels: " << engine.getOutputChannelCount (n) << "\n";
    std::cout << "\nCurrent input:  " << engine.getCurrentInputDevice() << "\n";
    std::cout << "Current output: " << engine.getCurrentOutputDevice() << "\n";
    return 0;
}

int doProbe (const Opts& o)
{
    std::cout << "\n=== PROBE (raw ground truth) ===\n";
    std::cout << "device~='" << o.device << "'  out=" << o.out << " (physical " << (o.out + 1)
              << ")  in=" << o.in << " (physical " << (o.in + 1) << ")\n";

    juce::AudioDeviceManager dm;
    juce::String err = dm.initialiseWithDefaultDevices (64, 64);
    if (err.isNotEmpty()) { std::cout << "init error: " << err << "\n"; return 1; }

    auto* type = dm.getCurrentDeviceTypeObject();
    if (type != nullptr) type->scanForDevices();

    // Pick the device by substring across input names (I/O aggregate on the same box).
    juce::StringArray inNames = type ? type->getDeviceNames (true) : juce::StringArray {};
    juce::StringArray outNames = type ? type->getDeviceNames (false) : juce::StringArray {};
    juce::String inDev = matchDevice (inNames, o.device);
    juce::String outDev = matchDevice (outNames, o.device);
    if (inDev.isEmpty() || outDev.isEmpty())
    {
        std::cout << "Could not find device matching '" << o.device << "'.\n"
                  << "Available inputs: " << inNames.joinIntoString (", ") << "\n"
                  << "Available outputs: " << outNames.joinIntoString (", ") << "\n";
        return 1;
    }

    auto setup = dm.getAudioDeviceSetup();
    setup.inputDeviceName = inDev;
    setup.outputDeviceName = outDev;
    setup.useDefaultInputChannels = true;   // let JUCE enable all (matches app behavior)
    setup.useDefaultOutputChannels = true;
    err = dm.setAudioDeviceSetup (setup, true);
    if (err.isNotEmpty()) { std::cout << "setup error: " << err << "\n"; return 1; }

    auto* dev = dm.getCurrentAudioDevice();
    if (dev == nullptr) { std::cout << "no device opened\n"; return 1; }

    std::cout << "Opened: in='" << dev->getName() << "' @ " << dev->getCurrentSampleRate() << " Hz\n";
    std::cout << "Active input channels:  " << dev->getActiveInputChannels().toString (2) << "\n";
    std::cout << "Active output channels: " << dev->getActiveOutputChannels().toString (2) << "\n";
    std::cout << "Input channel names ("  << dev->getInputChannelNames().size()  << "): "
              << dev->getInputChannelNames().joinIntoString (", ") << "\n";

    ProbeCallback cb (o.out);
    cb.sampleRate = dev->getCurrentSampleRate();
    dm.addAudioCallback (&cb);
    std::cout << "Playing " << kToneFreq << " Hz tone for " << o.secs << " s...\n";
    juce::Thread::sleep ((int) (o.secs * 1000.0));
    dm.removeAudioCallback (&cb);

    std::cout << "\nPer-input-channel RMS (" << cb.capturedInputChans << " channels captured):\n";
    double maxRms = 0.0; int maxCh = -1;
    for (size_t ch = 0; ch < cb.sumSquares.size(); ++ch)
    {
        double rms = cb.totalSamples > 0 ? std::sqrt (cb.sumSquares[ch] / (double) cb.totalSamples) : 0.0;
        if (rms > maxRms) { maxRms = rms; maxCh = (int) ch; }
        bool isTarget = ((int) ch == o.in);
        if (rms > 1.0e-5 || isTarget || cb.channelNull[ch])
        {
            std::cout << "  ch " << pad (juce::String ((int) ch), 3)
                      << " (physical " << pad (juce::String ((int) ch + 1), 3) << ")  "
                      << pad (dbfs ((float) rms), 10)
                      << (cb.channelNull[ch] ? "  [NULL PTR]" : "")
                      << (isTarget ? "   <== target input" : "")
                      << ((int) ch == maxCh && rms > 1.0e-5 ? "" : "")
                      << "\n";
        }
    }
    std::cout << "\nLoudest input channel: ";
    if (maxCh >= 0) std::cout << "ch " << maxCh << " (physical " << (maxCh + 1) << ") at " << dbfs ((float) maxRms) << "\n";
    else std::cout << "NONE - all inputs silent (check mic permission / routing)\n";

    if (o.in >= 0 && o.in < (int) cb.sumSquares.size())
    {
        double targetRms = std::sqrt (cb.sumSquares[(size_t) o.in] / (double) cb.totalSamples);
        std::cout << "Target input ch " << o.in << ": " << dbfs ((float) targetRms)
                  << (targetRms > 1.0e-4 ? "   ROUNDTRIP OK" : "   NO SIGNAL") << "\n";
    }
    else
    {
        std::cout << "Target input ch " << o.in << " is OUT OF RANGE (only "
                  << cb.sumSquares.size() << " channels active)\n";
    }
    return 0;
}

// Demonstrates bug #2's root cause: the channel count reported for a device is
// the default fallback (2) until the device is actually opened. The UI populated
// its channel dropdown BEFORE opening, capping it at 2 channels. Opening first
// (the fix) yields the true count.
int doVerify (const Opts& o)
{
    std::cout << "\n=== VERIFY channel-count ordering (bug #2 root cause) ===\n";
    AudioEngine engine;
    if (! engine.initialize()) { std::cout << "ERROR: init failed\n"; return 1; }

    auto inDev = matchDevice (engine.getInputDeviceNames(), o.device);
    auto outDev = matchDevice (engine.getOutputDeviceNames(), o.device);
    if (inDev.isEmpty()) { std::cout << "device not found\n"; return 1; }

    int inBefore = engine.getInputChannelCount (inDev);
    int outBefore = engine.getOutputChannelCount (outDev);
    std::cout << "BEFORE opening '" << inDev << "':\n";
    std::cout << "  getInputChannelCount  = " << inBefore
              << (inBefore <= 2 ? "   (BUG: dropdown would cap here - input " + juce::String (o.in + 1) + " unselectable)" : "") << "\n";
    std::cout << "  getOutputChannelCount = " << outBefore << "\n";

    // The fix: open the device (setInputDevice/setOutputDevice) BEFORE querying.
    engine.setOutputDevice (outDev, 0);
    engine.setInputDevice (inDev, 0);

    int inAfter = engine.getInputChannelCount (inDev);
    int outAfter = engine.getOutputChannelCount (outDev);
    std::cout << "AFTER setInputDevice/setOutputDevice (the fix - open first):\n";
    std::cout << "  getInputChannelCount  = " << inAfter
              << (inAfter > o.in ? "   (OK: input " + juce::String (o.in + 1) + " now selectable)" : "") << "\n";
    std::cout << "  getOutputChannelCount = " << outAfter << "\n";

    bool fixed = inAfter > o.in && outAfter > o.out;
    std::cout << "\n" << (fixed ? "PASS - real channel counts exposed once the device is opened first"
                                 : "FAIL - channel count still insufficient") << "\n";
    return fixed ? 0 : 1;
}

int doEngine (const Opts& o)
{
    std::cout << "\n=== ENGINE (real AudioEngine path) ===\n";
    AudioEngine engine;
    if (! engine.initialize()) { std::cout << "ERROR: init failed\n"; return 1; }

    auto inDev = matchDevice (engine.getInputDeviceNames(), o.device);
    auto outDev = matchDevice (engine.getOutputDeviceNames(), o.device);
    std::cout << "Matched input device:  '" << inDev << "'\n";
    std::cout << "Matched output device: '" << outDev << "'\n";
    if (inDev.isEmpty() || outDev.isEmpty()) { std::cout << "device not found\n"; return 1; }

    bool okOut = engine.setOutputDevice (outDev, o.out);
    bool okIn = engine.setInputDevice (inDev, o.in);
    std::cout << "setOutputDevice -> " << (okOut ? "true" : "FALSE")
              << " ; setInputDevice -> " << (okIn ? "true" : "FALSE") << "\n";
    std::cout << "Engine reports: inputDev='" << engine.getCurrentInputDevice()
              << "' inputCh=" << engine.getCurrentInputChannel()
              << "  outputDev='" << engine.getCurrentOutputDevice()
              << "' outputCh=" << engine.getCurrentOutputChannel()
              << "  sr=" << engine.getCurrentSampleRate() << "\n";

    // 1) Test tone + input meter poll (meter values are already in dBFS)
    engine.setTestToneEnabled (true);
    engine.resetPeakHold();
    int polls = juce::jmax (1, (int) (o.secs * 20));
    for (int i = 0; i < polls; ++i)
        juce::Thread::sleep (50);
    auto mv = engine.getInputMeterValues();
    std::cout << "\nInput meter after tone: rms=" << juce::String (mv.rmsDb, 1)
              << " dB  peak=" << juce::String (mv.peakDb, 1)
              << " dB  peakHold=" << juce::String (mv.peakHoldDb, 1) << " dB\n";

    // 2) Record the input to a WAV under build/ and analyze it (exercises the
    //    real recording flow used by capture).
    juce::File out = juce::File::getCurrentWorkingDirectory()
                         .getChildFile ("build")
                         .getChildFile ("harness_capture.wav");
    out.getParentDirectory().createDirectory();
    auto rec = engine.startRecording (out);
    std::cout << "startRecording -> " << (rec.success ? "true" : "FALSE")
              << (rec.errorMessage.isNotEmpty() ? (" (" + rec.errorMessage + ")") : juce::String()) << "\n";
    juce::Thread::sleep ((int) (o.secs * 1000.0));
    engine.stopRecording();
    engine.setTestToneEnabled (false);

    // Analyze the recorded file
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (out));
    if (reader != nullptr && reader->lengthInSamples > 0)
    {
        juce::AudioBuffer<float> buf (1, (int) reader->lengthInSamples);
        reader->read (&buf, 0, (int) reader->lengthInSamples, 0, true, false);
        float rms = buf.getRMSLevel (0, 0, buf.getNumSamples());
        float mag = buf.getMagnitude (0, 0, buf.getNumSamples());
        std::cout << "Recorded " << reader->lengthInSamples << " samples @ " << reader->sampleRate
                  << " Hz -> rms=" << dbfs (rms) << "  peak=" << dbfs (mag) << "\n";
        std::cout << (mag > 1.0e-3f ? "RECORDING OK - input signal captured\n"
                                    : "RECORDING SILENT - no input signal reached the recorder\n");
    }
    else
    {
        std::cout << "Could not read recorded file (empty or missing)\n";
    }
    return 0;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::StringArray args;
    for (int i = 1; i < argc; ++i) args.add (juce::String (argv[i]));

    juce::String cmd = args.isEmpty() ? "engine" : args[0];
    Opts o = parseOpts (args);

    std::cout << "EngineHarness\n";
    if (cmd == "list") return doList();
    if (cmd == "probe") return doProbe (o);
    if (cmd == "engine") return doEngine (o);
    if (cmd == "verify") return doVerify (o);

    std::cout << "Usage: EngineHarness <list|probe|engine|verify> [--device <substr>] [--out <idx>] [--in <idx>] [--secs <f>]\n";
    return 2;
}
