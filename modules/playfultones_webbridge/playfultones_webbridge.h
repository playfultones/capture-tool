/** BEGIN_JUCE_MODULE_DECLARATION
ID:               playfultones_webbridge
vendor:           Playful Tones
version:          1.0.0
name:             WebView Bridge
description:      Base infrastructure for WebView native function registration and event emission.
website:          https://playfultones.com
license:          AGPLv3
dependencies:     juce_gui_extra
END_JUCE_MODULE_DECLARATION
*/
#pragma once
#define PLAYFULTONES_WEBBRIDGE_H_INCLUDED

#include <juce_gui_extra/juce_gui_extra.h>

#include "src/NativeBridge.h"
