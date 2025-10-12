#pragma once

#include <stdint.h>
#include <ar/ar_defines.h>

#include <utils/id.h>
#include <utils/buffer.h>

// clang-format off

AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_copy<::std::int8_t>,
                      deserialize_handler = ::agea::reflection::utils::cpp_deserialize<::std::int8_t>,
                      serialize_handler   = ::agea::reflection::utils::cpp_serialize<::std::int8_t>);
AGEA_ar_external_define(::std::int8_t);


AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_copy<::std::int16_t>,
                      deserialize_handler = ::agea::reflection::utils::cpp_deserialize<::std::int16_t>,
                      serialize_handler   = ::agea::reflection::utils::cpp_serialize<::std::int16_t>);
AGEA_ar_external_define(::std::int16_t);


AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_copy<::std::int32_t>,
                      deserialize_handler = ::agea::reflection::utils::cpp_deserialize<::std::int32_t>,
                      serialize_handler   = ::agea::reflection::utils::cpp_serialize<::std::int32_t>);
AGEA_ar_external_define(::std::int32_t);


AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_copy<::std::int64_t>,
                      deserialize_handler = ::agea::reflection::utils::cpp_deserialize<::std::int64_t>,
                      serialize_handler   = ::agea::reflection::utils::cpp_serialize<::std::int64_t>);
AGEA_ar_external_define(::std::int64_t);


AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_copy<::std::uint8_t>,
                      deserialize_handler = ::agea::reflection::utils::cpp_deserialize<::std::uint8_t>,
                      serialize_handler   = ::agea::reflection::utils::cpp_serialize<::std::uint8_t>);
AGEA_ar_external_define(::std::uint8_t);


AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_copy<::std::uint16_t>,
                      deserialize_handler = ::agea::reflection::utils::cpp_deserialize<::std::uint16_t>,
                      serialize_handler   = ::agea::reflection::utils::cpp_serialize<::std::uint16_t>);
AGEA_ar_external_define(::std::uint16_t);


AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_copy<::std::uint32_t>,
                      deserialize_handler = ::agea::reflection::utils::cpp_deserialize<::std::uint32_t>,
                      serialize_handler   = ::agea::reflection::utils::cpp_serialize<::std::uint32_t>);
AGEA_ar_external_define(::std::uint32_t);


AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_copy<::std::uint64_t>,
                      deserialize_handler = ::agea::reflection::utils::cpp_deserialize<::std::uint64_t>,
                      serialize_handler   = ::agea::reflection::utils::cpp_serialize<::std::uint64_t>);
AGEA_ar_external_define(::std::uint64_t);


AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_copy<bool>,
                      deserialize_handler = ::agea::reflection::utils::cpp_deserialize<bool>,
                      serialize_handler   = ::agea::reflection::utils::cpp_serialize<bool>,
                      built_in            = true);
AGEA_ar_external_define(bool);


AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_copy<float>,
                      deserialize_handler = ::agea::reflection::utils::cpp_deserialize<float>,
                      serialize_handler   = ::agea::reflection::utils::cpp_serialize<float>,
                      built_in            = true);
AGEA_ar_external_define(float);


AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_copy<double>,
                      deserialize_handler = ::agea::reflection::utils::cpp_deserialize<double>,
                      serialize_handler   = ::agea::reflection::utils::cpp_serialize<double>,
                      built_in            = true);
AGEA_ar_external_define(double);


AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_copy<::std::string>,
                      deserialize_handler = ::agea::reflection::utils::cpp_deserialize<::std::string>,
                      serialize_handler   = ::agea::reflection::utils::cpp_serialize<::std::string>);
AGEA_ar_external_define(::std::string);


AGEA_ar_external_type(to_string_handler   = buffer__to_string,
                      copy_handler        = buffer__copy,
                      deserialize_handler = buffer__deserialize,
                      serialize_handler   = buffer__serialize);
AGEA_ar_external_define(::agea::utils::buffer);


AGEA_ar_external_type(to_string_handler   = id__to_string,
                      deserialize_handler = id__deserialize,
                      serialize_handler   = id__serialize);
AGEA_ar_external_define(::agea::utils::id);


//clang-format on