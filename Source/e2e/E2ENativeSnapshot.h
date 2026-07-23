#pragma once

#if REFCAP_E2E

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

/**
    Takes a pixel-accurate snapshot of the WKWebView hosted inside the given
    component's window, using WKWebView's own snapshot API. JUCE's
    createComponentSnapshot() cannot see native child views (it renders the
    JUCE paint tree only), and screencapture(1) needs a Screen Recording TCC
    grant; this needs neither.

    The callback is invoked on the message thread with a standard base64-encoded
    PNG on success, or an empty image string and a non-empty error.
*/
void e2eTakeNativeSnapshot(juce::Component& componentInTargetWindow,
                           std::function<void(juce::String base64Png, juce::String error)> done);

#endif
