#pragma once

#include <stdint.h>
#include <ar/ar_defines.h>

#include <utils/id.h>
#include <utils/buffer.h>

// clang-format off

AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_default__copy<::std::int8_t>,
                      load_derive_handler = ::agea::reflection::utils::cpp_default__load<::std::int8_t>,
                      serialize_handler   = ::agea::reflection::utils::cpp_default__save<::std::int8_t>,
                      compare_handler     = ::agea::reflection::utils::cpp_default__compare<::std::int8_t>);
AGEA_ar_external_define(::std::int8_t);


AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_default__copy<::std::int16_t>,
                      load_derive_handler = ::agea::reflection::utils::cpp_default__load<::std::int16_t>,
                      serialize_handler   = ::agea::reflection::utils::cpp_default__save<::std::int16_t>,
                      compare_handler     = ::agea::reflection::utils::cpp_default__compare<::std::int16_t>);
AGEA_ar_external_define(::std::int16_t);


AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_default__copy<::std::int32_t>,
                      load_derive_handler = ::agea::reflection::utils::cpp_default__load<::std::int32_t>,
                      serialize_handler   = ::agea::reflection::utils::cpp_default__save<::std::int32_t>,
                      compare_handler     = ::agea::reflection::utils::cpp_default__compare<::std::int32_t>);
AGEA_ar_external_define(::std::int32_t);


AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_default__copy<::std::int64_t>,
                      load_derive_handler = ::agea::reflection::utils::cpp_default__load<::std::int64_t>,
                      serialize_handler   = ::agea::reflection::utils::cpp_default__save<::std::int64_t>,
                      compare_handler     = ::agea::reflection::utils::cpp_default__compare<::std::int64_t>);
AGEA_ar_external_define(::std::int64_t);


AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_default__copy<::std::uint8_t>,
                      load_derive_handler = ::agea::reflection::utils::cpp_default__load<::std::uint8_t>,
                      serialize_handler   = ::agea::reflection::utils::cpp_default__save<::std::uint8_t>,
                      compare_handler     = ::agea::reflection::utils::cpp_default__compare<::std::uint8_t>);
AGEA_ar_external_define(::std::uint8_t);


AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_default__copy<::std::uint16_t>,
                      load_derive_handler = ::agea::reflection::utils::cpp_default__load<::std::uint16_t>,
                      serialize_handler   = ::agea::reflection::utils::cpp_default__save<::std::uint16_t>,
                      compare_handler     = ::agea::reflection::utils::cpp_default__compare<::std::uint16_t>);
AGEA_ar_external_define(::std::uint16_t);


AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_default__copy<::std::uint32_t>,
                      load_derive_handler = ::agea::reflection::utils::cpp_default__load<::std::uint32_t>,
                      serialize_handler   = ::agea::reflection::utils::cpp_default__save<::std::uint32_t>,
                      compare_handler     = ::agea::reflection::utils::cpp_default__compare<::std::uint32_t>);
AGEA_ar_external_define(::std::uint32_t);


AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_default__copy<::std::uint64_t>,
                      load_derive_handler = ::agea::reflection::utils::cpp_default__load<::std::uint64_t>,
                      serialize_handler   = ::agea::reflection::utils::cpp_default__save<::std::uint64_t>,
                      compare_handler     = ::agea::reflection::utils::cpp_default__compare<::std::uint64_t>);
AGEA_ar_external_define(::std::uint64_t);


AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_default__copy<bool>,
                      load_derive_handler = ::agea::reflection::utils::cpp_default__load<bool>,
                      serialize_handler   = ::agea::reflection::utils::cpp_default__save<bool>,
                      compare_handler     = ::agea::reflection::utils::cpp_default__compare<bool>,
                      built_in            = true);
AGEA_ar_external_define(bool);


AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_default__copy<float>,
                      load_derive_handler = ::agea::reflection::utils::cpp_default__load<float>,
                      serialize_handler   = ::agea::reflection::utils::cpp_default__save<float>,
                      compare_handler     = ::agea::reflection::utils::cpp_default__compare<float>,
                      built_in            = true);
AGEA_ar_external_define(float);


AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_default__copy<double>,
                      load_derive_handler = ::agea::reflection::utils::cpp_default__load<double>,
                      serialize_handler   = ::agea::reflection::utils::cpp_default__save<double>,
                      compare_handler     = ::agea::reflection::utils::cpp_default__compare<double>,
                      built_in            = true);
AGEA_ar_external_define(double);


AGEA_ar_external_type(copy_handler        = ::agea::reflection::utils::cpp_default__copy<::std::string>,
                      load_derive_handler = ::agea::reflection::utils::cpp_default__load<::std::string>,
                      serialize_handler   = ::agea::reflection::utils::cpp_default__save<::std::string>);
AGEA_ar_external_define(::std::string);


AGEA_ar_external_type(to_string_handler   = buffer__to_string,
                      copy_handler        = buffer__copy,
                      load_derive_handler = buffer__load,
                      serialize_handler   = buffer__save);
AGEA_ar_external_define(::agea::utils::buffer);


AGEA_ar_external_type(to_string_handler   = id__to_string,
                      load_derive_handler = id__load,
                      serialize_handler   = id__save);
AGEA_ar_external_define(::agea::utils::id);


//clang-format on