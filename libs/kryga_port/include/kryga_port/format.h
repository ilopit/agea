#pragma once

// Portability wrapper for <format>. NDK r26d ships libc++ 17, which does not
// expose std::format at any language level. Route to spdlog's bundled fmt on
// Android and inject the symbols into namespace std so existing call sites
// compile unchanged. Injecting into namespace std is technically UB but is a
// pragmatic shim while Android's libc++ catches up.
//
// Include this header instead of <format>.

#if defined(__ANDROID__)

#include <spdlog/fmt/fmt.h>

namespace std
{
using fmt::format;
using fmt::format_to;
using fmt::vformat;
using fmt::vformat_to;
using fmt::format_string;
}  // namespace std

#else

#include <format>

#endif
