#pragma once

#include "engine/RhythmGenerator.h"

namespace livelooper
{
/** The drum samples compiled into livelooper (resources/drums, CC0 / public domain), decoded
    once per process and shared by every instance. Never returns null (empty bank on failure). */
std::shared_ptr<const DrumBank> getEmbeddedDrumBank();
}
