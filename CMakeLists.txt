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
)

# Collect all source files from Source/ directory
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
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
