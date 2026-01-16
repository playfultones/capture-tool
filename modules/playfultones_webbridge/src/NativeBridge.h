#pragma once

#include <functional>
#include <map>

namespace playfultones
{

/**
 * Alias for native function completion callback.
 */
using NativeCompletion = juce::WebBrowserComponent::NativeFunctionCompletion;

/**
 * Alias for native function handler signature.
 * 
 * @param args Arguments passed from JavaScript
 * @param complete Callback to return a result to JavaScript
 */
using NativeHandler = std::function<void(const juce::Array<juce::var>& args, NativeCompletion complete)>;

/**
 * Base class for managing WebView native function registration.
 * 
 * This class provides a centralized way to register native functions
 * that can be called from JavaScript and emit events back to the frontend.
 * 
 * Usage:
 *   1. Create a NativeBridge instance
 *   2. Call registerFunction() for each native function
 *   3. Call applyTo() when creating WebBrowserComponent::Options
 *   4. Use emitEvent() to send events to JavaScript
 * 
 * Example:
 *   NativeBridge bridge;
 *   bridge.registerFunction("getVersion", [](auto& args, auto complete) {
 *       complete(juce::var("1.0.0"));
 *   });
 *   
 *   auto options = bridge.applyTo(WebBrowserComponent::Options{});
 */
class NativeBridge
{
public:
    NativeBridge() = default;
    ~NativeBridge() = default;

    /**
     * Register a native function that can be called from JavaScript.
     * 
     * @param name Function name (will be available as window.__JUCE__.backend.name)
     * @param handler Function that handles the call
     */
    void registerFunction(const juce::String& name, NativeHandler handler);

    /**
     * Remove a registered function.
     * 
     * @param name Function name to remove
     */
    void unregisterFunction(const juce::String& name);

    /**
     * Check if a function is registered.
     * 
     * @param name Function name to check
     * @return true if registered
     */
    bool hasFunction(const juce::String& name) const;

    /**
     * Get the number of registered functions.
     */
    int getNumFunctions() const;

    /**
     * Apply all registered functions to WebBrowserComponent::Options.
     * Call this when creating the WebBrowserComponent.
     * 
     * @param options Options to modify
     * @return Modified options with all native functions added
     */
    juce::WebBrowserComponent::Options applyTo(juce::WebBrowserComponent::Options options) const;

    /**
     * Emit an event to the WebView.
     * The event will be received by JavaScript event listeners.
     * 
     * @param webView The WebBrowserComponent to emit to
     * @param eventId Event identifier
     * @param data Event data (can be any juce::var)
     */
    static void emitEvent(juce::WebBrowserComponent* webView,
                          const juce::Identifier& eventId,
                          const juce::var& data);

    /**
     * Emit an event only if the browser is visible.
     * 
     * @param webView The WebBrowserComponent to emit to
     * @param eventId Event identifier
     * @param data Event data
     */
    static void emitEventIfVisible(juce::WebBrowserComponent* webView,
                                   const juce::Identifier& eventId,
                                   const juce::var& data);

private:
    std::map<juce::String, NativeHandler> handlers;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NativeBridge)
};

} // namespace playfultones
