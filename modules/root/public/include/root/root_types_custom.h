#pragma once

#include <stdint.h>
#include <ar/ar_defines.h>

#include <utils/id.h>
#include <utils/buffer.h>
#include <root/core_types/vec2.h>
#include <root/core_types/vec3.h>
#include <root/core_types/vec4.h>

AGEA_ar_type(std::int8_t);
AGEA_ar_type(std::int16_t);
AGEA_ar_type(std::int32_t);
AGEA_ar_type(std::int64_t);

AGEA_ar_type(std::uint8_t);
AGEA_ar_type(std::uint16_t);
AGEA_ar_type(std::uint32_t);
AGEA_ar_type(std::uint64_t);

AGEA_ar_type(bool);
AGEA_ar_type(float);
AGEA_ar_type(double);
AGEA_ar_type(std::string);

AGEA_ar_type(::agea::utils::buffer);
AGEA_ar_type(::agea::utils::id);

AGEA_ar_type(::agea::root::vec2);
AGEA_ar_type(::agea::root::vec3);
AGEA_ar_type(::agea::root::vec4);