#pragma once

#include <functional>

/**
 * macOS microphone permission helper.
 *
 * JUCE's juce_audio_devices does NOT request microphone (TCC) access on macOS -
 * it just opens the CoreAudio device and relies on the OS to prompt implicitly.
 * For many builds (notably ad-hoc signed / non-notarized dev builds) that implicit
 * prompt never appears and CoreAudio silently returns zeros. We therefore request
 * access explicitly via AVFoundation, which reliably shows the system prompt the
 * first time and resolves immediately thereafter.
 */
namespace macpermissions
{
    enum class MicStatus
    {
        notDetermined, // user has not been asked yet - requesting will show the prompt
        restricted,    // access disallowed by policy (e.g. parental controls / MDM)
        denied,        // user previously denied - must be changed in System Settings
        granted,       // access allowed
        unsupported    // not applicable on this platform
    };

    /** Returns the current microphone authorization status. */
    MicStatus getMicrophoneStatus();

    /** Human-readable status, for logging. */
    const char* toString(MicStatus status);

    /**
     * Request microphone access. If the status is notDetermined this shows the
     * system permission prompt; otherwise it resolves immediately with the
     * existing decision. The callback receives whether access is granted and may
     * be invoked on an arbitrary thread. On non-Apple platforms it reports granted.
     */
    void requestMicrophoneAccess(std::function<void(bool)> callback);
}
