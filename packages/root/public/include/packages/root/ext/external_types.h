#pragma once

#include <stdint.h>
#include <ar/ar_defines.h>

#include <utils/id.h>
#include <utils/buffer.h>

AGEA_ar_external_type(::std::int8_t, "use-default-handlers");
AGEA_ar_external_type(::std::int16_t, "use-default-handlers");
AGEA_ar_external_type(::std::int32_t, "use-default-handlers");
AGEA_ar_external_type(::std::int64_t, "use-default-handlers");

AGEA_ar_external_type(::std::uint8_t, "use-default-handlers");
AGEA_ar_external_type(::std::uint16_t, "use-default-handlers");
AGEA_ar_external_type(::std::uint32_t, "use-default-handlers");
AGEA_ar_external_type(::std::uint64_t, "use-default-handlers");

AGEA_ar_external_type(bool, "built-in", "use-default-handlers");
AGEA_ar_external_type(float, "built-in", "use-default-handlers");
AGEA_ar_external_type(double, "built-in", "use-default-handlers");
AGEA_ar_external_type(::std::string, "use-default-handlers");

AGEA_ar_external_type(::agea::utils::buffer,
                      "use-script-support=true",
                      to_string_handler = custom::to_string_t_buf,
                      copy_handler = custom::copy_t_buf,
                      deserialize_handler = custom::deserialize_t_buf,
                      serialize_handler = custom::serialize_t_buf);
AGEA_ar_external_type(::agea::utils::id, "use-script-support=true");