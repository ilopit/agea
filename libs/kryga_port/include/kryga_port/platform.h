#pragma once

#include <kryga_port/build_tier.h>

// Platform identification — single source of truth for which OS/form-factor we
// target. Code should branch on KRG_DESKTOP / KRG_MOBILE (form factor) and on
// KRG_PLATFORM_* (specific OS), not on raw __ANDROID__/_WIN32/etc.
//
// Exactly one of KRG_DESKTOP / KRG_MOBILE is defined to 1.
// Exactly one of KRG_PLATFORM_WIN32 / KRG_PLATFORM_LINUX / KRG_PLATFORM_APPLE /
// KRG_PLATFORM_ANDROID is defined to 1.

#if defined(__ANDROID__)
#define KRG_PLATFORM_ANDROID 1
#define KRG_MOBILE 1
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#define KRG_PLATFORM_APPLE 1
#define KRG_MOBILE 1
#else
#define KRG_PLATFORM_APPLE 1
#define KRG_DESKTOP 1
#endif
#elif defined(_WIN32)
#define KRG_PLATFORM_WIN32 1
#define KRG_DESKTOP 1
#elif defined(__linux__)
#define KRG_PLATFORM_LINUX 1
#define KRG_DESKTOP 1
#else
#error "Unknown platform — extend kryga_port/platform.h"
#endif

// Sanity: at least one of each axis must be set.
#if !defined(KRG_DESKTOP) && !defined(KRG_MOBILE)
#error "Neither KRG_DESKTOP nor KRG_MOBILE set"
#endif
