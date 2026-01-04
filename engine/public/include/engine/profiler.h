#pragma once

#include <tracy/Tracy.hpp>

// Tracy zone macros
#define KRG_PROFILE_SCOPE(name) ZoneScopedN(name)
#define KRG_PROFILE_FUNCTION() ZoneScoped

// Frame mark for Tracy
#define KRG_PROFILE_FRAME_MARK() FrameMark

// GPU profiling (requires Vulkan context setup)
#define KRG_PROFILE_GPU_ZONE(ctx, name) TracyVkZone(ctx, name)
#define KRG_PROFILE_GPU_COLLECT(ctx) TracyVkCollect(ctx)

// Memory tracking
#define KRG_PROFILE_ALLOC(ptr, size) TracyAlloc(ptr, size)
#define KRG_PROFILE_FREE(ptr) TracyFree(ptr)

// Plot values
#define KRG_PROFILE_PLOT(name, value) TracyPlot(name, value)

// Message/log
#define KRG_PROFILE_MESSAGE(msg) TracyMessage(msg, strlen(msg))
#define KRG_PROFILE_MESSAGE_L(msg) TracyMessageL(msg)
