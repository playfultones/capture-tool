/** BEGIN_JUCE_MODULE_DECLARATION
ID:               playfultones_metering
vendor:           Playful Tones
version:          1.0.0
name:             Audio Metering
description:      Thread-safe audio level metering with RMS and peak calculation.
website:          https://playfultones.com
license:          AGPLv3
dependencies:     juce_core, juce_audio_basics
END_JUCE_MODULE_DECLARATION
*/
#pragma once
#define PLAYFULTONES_METERING_H_INCLUDED

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "src/LevelMeter.h"
