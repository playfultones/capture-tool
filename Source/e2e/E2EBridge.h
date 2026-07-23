#pragma once

#if REFCAP_E2E

#include <juce_gui_extra/juce_gui_extra.h>
#include <focusrite/e2e/CommandHandler.h>
#include <focusrite/e2e/TestCentre.h>

/**
    Dev-only bridge for end-to-end automation (CMake option E2E_TESTING).

    Hosts a focusrite::e2e::TestCentre and adds an "evaluate-js" command that
    runs a script inside the WebView UI. The evaluation is asynchronous, so the
    command is acked immediately and the result arrives as a "js-response"
    event carrying the caller-supplied "id" for correlation.
*/
class E2EBridge : private focusrite::e2e::CommandHandler
{
public:
    explicit E2EBridge(juce::WebBrowserComponent& webViewToDrive);
    ~E2EBridge() override;

    /** True when the app was launched by the e2e driver (--e2e-test-port=N). */
    static bool isRequested();

private:
    std::optional<focusrite::e2e::Response> process(const focusrite::e2e::Command& command) override;

    juce::WebBrowserComponent& webView;
    std::unique_ptr<focusrite::e2e::TestCentre> testCentre;
};

#endif
