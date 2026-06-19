#pragma once

#include <tracy/Tracy.hpp>

// Tracy zone macros
#define KRG_PROFILE_SCOPE(name) ZoneScopedN(name)

// Frame mark for Tracy
#define KRG_PROFILE_FRAME_MARK() FrameMark
