cmake_minimum_required(VERSION 3.22)

# Get version from git tags
include(cmake/GitVersion.cmake)

project(ReferenceCapturer VERSION ${CURRENT_VERSION})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add JUCE from submodule
add_subdirectory(JUCE)

# Add custom JUCE modules
juce_add_module(modules/playfultones_metering)
juce_add_module(modules/playfultones_wavrecorder)
juce_add_module(modules/playfultones_jsonhelpers)
juce_add_module(modules/playfultones_webbridge)

juce_add_gui_app(ReferenceCapturer
    PRODUCT_NAME "Reference Capturer"
    BUNDLE_ID "com.playfultones.referencecapturer"
    COMPANY_NAME "Playful Tones"
    ICON_BIG "${CMAKE_SOURCE_DIR}/Resources/icons/icon_big.png"
    ICON_SMALL "${CMAKE_SOURCE_DIR}/Resources/icons/icon_small.png"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Reference Capturer needs microphone access to capture audio from your hardware."
    CAMERA_PERMISSION_ENABLED TRUE
    CAMERA_PERMISSION_TEXT "Reference Capturer needs camera access for the visual capture guide overlay feature."
    # The *_PERMISSION_ENABLED flags only add the Info.plist usage strings. Under
    # hardened runtime (which scripts/sign-app.sh applies via --options runtime for
    # the Developer ID build), macOS ALSO requires the matching resource-access
    # entitlements or it hard-blocks the device with no prompt. JUCE generates the
    # entitlements file from HARDENED_RUNTIME_OPTIONS, so declare them here — this
    # is what makes mic/camera work when launched from /Applications, not just the
    # ad-hoc build folder copy (which has no hardened runtime and is exempt).
    HARDENED_RUNTIME_ENABLED TRUE
    HARDENED_RUNTIME_OPTIONS
        "com.apple.security.device.audio-input"
        "com.apple.security.device.camera"
)

# Collect all source files from Source/ directory
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.mm"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h"
)

target_sources(ReferenceCapturer
    PRIVATE
        ${SOURCE_FILES}
)

target_compile_definitions(ReferenceCapturer
    PRIVATE
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:ReferenceCapturer,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:ReferenceCapturer,JUCE_VERSION>"
)

target_link_libraries(ReferenceCapturer
    PRIVATE
        # macOS microphone (TCC) permission request via AVFoundation
        "-framework AVFoundation"
        # Custom modules
        playfultones_metering
        playfultones_wavrecorder
        playfultones_jsonhelpers
        playfultones_webbridge
        # JUCE modules
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

#==============================================================================
# E2E automation bridge (dev only): juce-end-to-end TestCentre + evaluate-js
# command for driving the WebView UI from the TypeScript test driver.
option(E2E_TESTING "Build with the end-to-end automation bridge" OFF)

if(E2E_TESTING)
    add_subdirectory(juce-end-to-end EXCLUDE_FROM_ALL)

    target_link_libraries(ReferenceCapturer PRIVATE focusrite-e2e)
    target_compile_definitions(ReferenceCapturer PRIVATE REFCAP_E2E=1)
endif()

#==============================================================================
# EngineHarness: headless CLI to verify the audio engine (roundtrip / recording)
# against real hardware, without the WebView/GUI. Not part of the shipped app.
juce_add_console_app(EngineHarness
    PRODUCT_NAME "EngineHarness"
)

target_sources(EngineHarness
    PRIVATE
        harness/main.cpp
        Source/AudioEngine.cpp
)

target_include_directories(EngineHarness PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")

target_compile_definitions(EngineHarness
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
        JUCE_APPLICATION_NAME_STRING="EngineHarness"
        JUCE_APPLICATION_VERSION_STRING="0.0.0"
)

target_link_libraries(EngineHarness
    PRIVATE
        playfultones_metering
        playfultones_wavrecorder
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Copy webapp resources to the app bundle
set(WEBAPP_SOURCE_DIR "${CMAKE_SOURCE_DIR}/Resources/webapp")
set(WEBAPP_DEST_DIR "$<TARGET_BUNDLE_CONTENT_DIR:ReferenceCapturer>/Resources/webapp")

add_custom_command(TARGET ReferenceCapturer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${WEBAPP_DEST_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${WEBAPP_SOURCE_DIR}" "${WEBAPP_DEST_DIR}"
    COMMENT "Copying webapp resources to bundle"
)

#==============================================================================
# Keep the build-folder bundle's code-signing IDENTITY stable across rebuilds.
#
# MUST BE THE LAST POST_BUILD COMMAND. CMake runs POST_BUILD commands in
# declaration order, and the webapp copy above writes into Contents/Resources,
# which the signature seals — signing before it would leave an invalid signature
# on any build where a webapp file actually changed. (It would appear to work
# whenever the JS was untouched, since copy_directory rewrites identical bytes
# and the resource hashes still match. A rebuild-after-JS-edit is what breaks.)
#
# Why sign at all here: the linker's default ad-hoc signature identifies the app
# as "Reference Capturer" (flags: adhoc,linker-signed) rather than by its bundle
# id. macOS TCC keys a microphone grant to the app's identity, so every rebuild
# produced a differently-identified binary and the mic permission had to be
# granted again. An ungranted mic does NOT raise an error — CoreAudio input
# silently returns zeros, which in a capture tool means a whole session of
# digital silence that looks like a successful run.
#
# Re-signing ad-hoc with the real bundle id plus the JUCE-generated entitlements
# reproduces the identity the previously-granted build had. No hardened runtime
# here — the build-folder copy is exempt, see the note on the target above.
# scripts/sign-app.sh replaces this with the Developer ID signature for dist.
if(APPLE)
    add_custom_command(TARGET ReferenceCapturer POST_BUILD
        COMMAND codesign --force --sign - --timestamp=none
                --identifier "com.playfultones.referencecapturer"
                --entitlements
                "${CMAKE_CURRENT_BINARY_DIR}/ReferenceCapturer_artefacts/JuceLibraryCode/ReferenceCapturer.entitlements"
                "$<TARGET_BUNDLE_DIR:ReferenceCapturer>"
        COMMAND codesign --verify --verbose=1 "$<TARGET_BUNDLE_DIR:ReferenceCapturer>"
        COMMENT "Ad-hoc re-signing with the bundle identifier (stable TCC identity)"
        VERBATIM)
endif()
