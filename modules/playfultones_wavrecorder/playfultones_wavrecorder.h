/** BEGIN_JUCE_MODULE_DECLARATION
ID:               playfultones_wavrecorder
vendor:           Playful Tones
version:          1.0.0
name:             WAV Recorder
description:      Threaded WAV file recording with lifecycle management.
website:          https://playfultones.com
license:          AGPLv3
dependencies:     juce_core, juce_audio_formats
END_JUCE_MODULE_DECLARATION
*/
#pragma once
#define PLAYFULTONES_WAVRECORDER_H_INCLUDED

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "src/ThreadedWavWriter.h"
