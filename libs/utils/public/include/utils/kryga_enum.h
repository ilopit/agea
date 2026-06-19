#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string_view>
#include <type_traits>

// Reflected enum generator.
//
// Declare the (id, initializer, name, label) row ONCE in an X-list macro, then
// KRG_declare_enum stamps out the enum class plus its string conversions:
//
//   #define MY_MODE_LIST(X)                              \
//       X(fifo,      0, "fifo",      "FIFO (vsync)")     \
//       X(mailbox,   1, "mailbox",   "Mailbox (low-lat)") \
//       X(immediate, 2, "immediate", "Immediate (tears)")
//
//   KRG_declare_enum(present_mode, uint32_t, MY_MODE_LIST)
//
// Row fields (4-field KRG_declare_enum):
//   id    — the C++ enumerator (e.g. pcf_3x3). Must be a valid identifier.
//   init  — the enumerator initializer: any constant expression, incl. macros
//           (e.g. KGPU_PCF_3X3). Values need NOT be contiguous.
//   name  — the STABLE serialization key, used for YAML/config/RPC. Decoupled
//           from `id` on purpose: rename the enumerator without breaking saved
//           files. May be anything ("3x3"), not just an identifier.
//   label — human-facing text for UI (combo boxes, etc.).
//
// For enums with no separate UI text (logging/debug/serialization-only), use the
// 3-field KRG_declare_enum_simple(EnumName, Underlying, LIST) where each row is
// X(id, init, name) and `label` defaults to `name`.
//
// Generates, in the enclosing namespace:
//   enum class present_mode : uint32_t { fifo = 0, ... };
//   struct  present_mode_info { present_mode value; const char* name; const char* label; };
//   constexpr std::array present_mode_entries{...};   // in list order
//   constexpr int present_mode_count;                 // entries.size()
//   const char* to_string(present_mode);              // -> name, "unknown" if absent
//   bool        from_string(std::string_view, present_mode&);   // matches name
//
// Plus, for free:
//   - a compile-time check that no two rows share a value or a name;
//   - formatter specializations so `std::format("{}", v)` AND spdlog
//     `ALOG_*("{}", v)` print the name directly on every platform — std::formatter
//     for desktop (std::format), fmt::formatter for Android (bundled fmt). No
//     to_string() needed at call sites.
//
// to_string/from_string are free functions found via ADL in the namespace where
// KRG_declare_enum is invoked. Lookups search the table by value/name, so they
// are correct for non-contiguous enums (these tables are tiny).
//
// SENTINEL/ALIAS VALUES (min/max/first/last/unknown): values that duplicate a
// real entry trip the duplicate-value static_assert, and they don't belong in
// the entries table / UI anyway. Keep them OUT of the X-list and declare them as
// `inline constexpr EnumName foo = EnumName::real_entry;` beside the enum.

// 4-field row appliers.
#define KRG_enum_x_enumerator(id, init, name, label) id = (init),
#define KRG_enum_x_row(id, init, name, label) {id, name, label},
// 3-field row appliers (label := name).
#define KRG_enum_x_enumerator3(id, init, name) id = (init),
#define KRG_enum_x_row3(id, init, name) {id, name, name},

// Shared body. ENUM_X / ROW_X are the row-applier macros matching LIST's arity.
#define KRG_declare_enum_with(EnumName, Underlying, LIST, ENUM_X, ROW_X)                 \
    enum class EnumName : Underlying                                                     \
    {                                                                                    \
        LIST(ENUM_X)                                                                     \
    };                                                                                   \
                                                                                         \
    struct EnumName##_info                                                               \
    {                                                                                    \
        EnumName value;                                                                  \
        const char* name;                                                                \
        const char* label;                                                               \
    };                                                                                   \
                                                                                         \
    inline constexpr auto EnumName##_entries = []                                        \
    {                                                                                    \
        using enum EnumName;                                                             \
        return std::to_array<EnumName##_info>({LIST(ROW_X)});                            \
    }();                                                                                 \
                                                                                         \
    inline constexpr int EnumName##_count = static_cast<int>(EnumName##_entries.size()); \
                                                                                         \
    static_assert(                                                                       \
        []                                                                               \
        {                                                                                \
            for (std::size_t i = 0; i < EnumName##_entries.size(); ++i)                  \
            {                                                                            \
                for (std::size_t j = i + 1; j < EnumName##_entries.size(); ++j)          \
                {                                                                        \
                    if (EnumName##_entries[i].value == EnumName##_entries[j].value)      \
                    {                                                                    \
                        return false;                                                    \
                    }                                                                    \
                    if (std::string_view(EnumName##_entries[i].name) ==                  \
                        std::string_view(EnumName##_entries[j].name))                    \
                    {                                                                    \
                        return false;                                                    \
                    }                                                                    \
                }                                                                        \
            }                                                                            \
            return true;                                                                 \
        }(),                                                                             \
        "KRG_declare_enum(" #EnumName "): duplicate value or name in the X-list");       \
                                                                                         \
    inline const char* to_string(EnumName v)                                             \
    {                                                                                    \
        for (auto& e : EnumName##_entries)                                               \
        {                                                                                \
            if (e.value == v)                                                            \
            {                                                                            \
                return e.name;                                                           \
            }                                                                            \
        }                                                                                \
        return "unknown";                                                                \
    }                                                                                    \
                                                                                         \
    inline bool from_string(std::string_view s, EnumName& out)                           \
    {                                                                                    \
        for (auto& e : EnumName##_entries)                                               \
        {                                                                                \
            if (s == e.name)                                                             \
            {                                                                            \
                out = e.value;                                                           \
                return true;                                                             \
            }                                                                            \
        }                                                                                \
        return false;                                                                    \
    }                                                                                    \
                                                                                         \
    /* ADL opt-in tag: only KRG_declare_enum types get the std::formatter below. */      \
    inline consteval bool krg_is_reflected_enum(EnumName)                                \
    {                                                                                    \
        return true;                                                                     \
    }

// Public entry points. 4-field rows (id, init, name, label):
#define KRG_declare_enum(EnumName, Underlying, LIST) \
    KRG_declare_enum_with(EnumName, Underlying, LIST, KRG_enum_x_enumerator, KRG_enum_x_row)
// 3-field rows (id, init, name); label defaults to name:
#define KRG_declare_enum_simple(EnumName, Underlying, LIST) \
    KRG_declare_enum_with(EnumName, Underlying, LIST, KRG_enum_x_enumerator3, KRG_enum_x_row3)

namespace kryga::enum_detail
{
// An enum is "reflected" iff KRG_declare_enum emitted its ADL marker. This keeps
// the blanket std::formatter specialization below from claiming unrelated enums.
template <class E>
concept reflected = std::is_enum_v<E> && requires(E e) {
    { krg_is_reflected_enum(e) } -> std::same_as<bool>;
};
}  // namespace kryga::enum_detail

// One specialization covers every reflected enum: formatting delegates to the
// ADL-found to_string(), so `std::format("{}", mode)` and spdlog `"{}"` print
// the serialization name. `to_string` and the marker are resolved at the format
// call site (after the enum is fully declared), so order here doesn't matter.
//
// Cross-platform: spdlog uses std::format on desktop but its bundled {fmt} on
// Android — see thirdparty/CMakeLists.txt, which sets SPDLOG_USE_STD_FORMAT =
// !ANDROID. The two formatter specializations are mutually exclusive on that
// exact condition: on Android <format>'s std::formatter<std::string_view> is
// absent, so the std specialization can't even compile there. If that CMake rule
// ever stops tracking ANDROID, update this guard to match.
#if !defined(__ANDROID__)
template <kryga::enum_detail::reflected E>
struct std::formatter<E> : std::formatter<std::string_view>
{
    template <class FmtContext>
    auto
    format(E v, FmtContext& ctx) const
    {
        return std::formatter<std::string_view>::format(to_string(v), ctx);
    }
};
#else
#include <spdlog/fmt/fmt.h>
template <kryga::enum_detail::reflected E>
struct fmt::formatter<E> : fmt::formatter<std::string_view>
{
    template <class FmtContext>
    auto
    format(E v, FmtContext& ctx) const
    {
        return fmt::formatter<std::string_view>::format(to_string(v), ctx);
    }
};
#endif
