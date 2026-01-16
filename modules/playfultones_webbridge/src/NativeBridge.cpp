#include "NativeBridge.h"

namespace playfultones
{

void NativeBridge::registerFunction(const juce::String& name, NativeHandler handler)
{
    handlers[name] = std::move(handler);
}

void NativeBridge::unregisterFunction(const juce::String& name)
{
    handlers.erase(name);
}

bool NativeBridge::hasFunction(const juce::String& name) const
{
    return handlers.find(name) != handlers.end();
}

int NativeBridge::getNumFunctions() const
{
    return static_cast<int>(handlers.size());
}

juce::WebBrowserComponent::Options NativeBridge::applyTo(
    juce::WebBrowserComponent::Options options) const
{
    for (const auto& pair : handlers)
    {
        const auto& name = pair.first;
        const auto& handler = pair.second;
        
        // Capture handler by value (it's a std::function, so this is a copy)
        options = options.withNativeFunction(
            name,
            [handler](const juce::Array<juce::var>& args, NativeCompletion complete)
            {
                handler(args, std::move(complete));
            });
    }

    return options;
}

void NativeBridge::emitEvent(juce::WebBrowserComponent* webView,
                             const juce::Identifier& eventId,
                             const juce::var& data)
{
    if (webView != nullptr)
        webView->emitEventIfBrowserIsVisible(eventId, data);
}

void NativeBridge::emitEventIfVisible(juce::WebBrowserComponent* webView,
                                       const juce::Identifier& eventId,
                                       const juce::var& data)
{
    if (webView != nullptr)
        webView->emitEventIfBrowserIsVisible(eventId, data);
}

} // namespace playfultones
