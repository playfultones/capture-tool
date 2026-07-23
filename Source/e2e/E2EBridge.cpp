#if REFCAP_E2E

#include "E2EBridge.h"
#include "E2ENativeSnapshot.h"

#include <focusrite/e2e/Command.h>
#include <focusrite/e2e/Event.h>
#include <focusrite/e2e/Response.h>

using focusrite::e2e::Command;
using focusrite::e2e::Event;
using focusrite::e2e::Response;
using focusrite::e2e::TestCentre;

E2EBridge::E2EBridge(juce::WebBrowserComponent& webViewToDrive)
    : webView(webViewToDrive),
      testCentre(TestCentre::create(TestCentre::LogLevel::verbose))
{
    testCentre->addCommandHandler(*this);
}

E2EBridge::~E2EBridge()
{
    testCentre->removeCommandHandler(*this);
}

bool E2EBridge::isRequested()
{
    for (const auto& param : juce::JUCEApplicationBase::getCommandLineParameterArray())
        if (param.startsWith("--e2e-test-port="))
            return true;

    return false;
}

std::optional<Response> E2EBridge::process(const Command& command)
{
    if (command.getType() == "native-screenshot")
    {
        const auto requestId = command.getArgument("id");

        e2eTakeNativeSnapshot(webView,
            [centre = testCentre.get(), requestId](juce::String base64Png, juce::String error)
            {
                auto event = Event("native-screenshot").withParameter("id", requestId);
                event = error.isNotEmpty() ? event.withParameter("error", error)
                                           : event.withParameter("image", base64Png);
                centre->sendEvent(event);
            });

        return Response::ok();
    }

    if (command.getType() != "evaluate-js")
        return std::nullopt;

    const auto requestId = command.getArgument("id");
    const auto script = command.getArgument("script");

    // Note: the callback captures the TestCentre raw pointer. If the app shuts
    // down with an evaluation in flight this dangles; acceptable for a
    // dev-only harness, the bridge outlives every driver interaction.
    webView.evaluateJavascript(script,
        [centre = testCentre.get(), requestId](juce::WebBrowserComponent::EvaluationResult result)
        {
            auto event = Event("js-response").withParameter("id", requestId);

            if (const auto* value = result.getResult())
            {
                // An undefined var (script with no return value) would be
                // serialised as the literal `undefined`, which is not JSON.
                event = event.withParameter(
                    "result", value->isUndefined() ? juce::var() : *value);
            }
            else if (const auto* error = result.getError())
                event = event.withParameter("error", error->message);

            centre->sendEvent(event);
        });

    return Response::ok();
}

#endif
