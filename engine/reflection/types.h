#pragma once

#include <type_traits>
#include <string>
#include <stdint.h>

#define AGEA_create_resolver(in_type, out_type) \
    template <>                                 \
    static supported_type resolve<in_type>()    \
    {                                           \
        return out_type;                        \
    }

namespace agea
{
namespace reflection
{

enum class access_mode
{
    ro = 0,
    rw
};

enum class supported_type
{
    t_nan = 0,

    t_str,

    t_i8,
    t_i16,
    t_i32,
    t_i64,

    t_u8,
    t_u16,
    t_u32,
    t_u64,

    t_f,
    t_d,
    t_last = t_d + 1
};

struct type_resolver
{
    template <typename T>
    static supported_type
    resolve()
    {
        return supported_type::t_nan;
    }

    AGEA_create_resolver(std::string, supported_type::t_str);

    AGEA_create_resolver(std::int8_t, supported_type::t_i8);
    AGEA_create_resolver(std::int16_t, supported_type::t_i16);
    AGEA_create_resolver(std::int32_t, supported_type::t_i32);
    AGEA_create_resolver(std::int64_t, supported_type::t_i64);

    AGEA_create_resolver(std::uint8_t, supported_type::t_u8);
    AGEA_create_resolver(std::uint16_t, supported_type::t_u16);
    AGEA_create_resolver(std::uint32_t, supported_type::t_u32);
    AGEA_create_resolver(std::uint64_t, supported_type::t_u64);

    AGEA_create_resolver(float, supported_type::t_f);
    AGEA_create_resolver(double, supported_type::t_d);
};

}  // namespace reflection
}  // namespace agea
