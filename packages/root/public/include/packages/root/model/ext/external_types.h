#pragma once

#include <cstdint>
#include <ar/ar_defines.h>

#include <utils/id.h>
#include <utils/buffer.h>

// clang-format off

KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::std::int8_t>,
                      load_derive_handler = ::kryga::reflection::utils::cpp_default__load<::std::int8_t>,
                      serialize_handler   = ::kryga::reflection::utils::cpp_default__save<::std::int8_t>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<::std::int8_t>);
KRG_ar_external_define(::std::int8_t);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::std::int16_t>,
                      load_derive_handler = ::kryga::reflection::utils::cpp_default__load<::std::int16_t>,
                      serialize_handler   = ::kryga::reflection::utils::cpp_default__save<::std::int16_t>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<::std::int16_t>);
KRG_ar_external_define(::std::int16_t);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::std::int32_t>,
                      load_derive_handler = ::kryga::reflection::utils::cpp_default__load<::std::int32_t>,
                      serialize_handler   = ::kryga::reflection::utils::cpp_default__save<::std::int32_t>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<::std::int32_t>);
KRG_ar_external_define(::std::int32_t);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::std::int64_t>,
                      load_derive_handler = ::kryga::reflection::utils::cpp_default__load<::std::int64_t>,
                      serialize_handler   = ::kryga::reflection::utils::cpp_default__save<::std::int64_t>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<::std::int64_t>);
KRG_ar_external_define(::std::int64_t);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::std::uint8_t>,
                      load_derive_handler = ::kryga::reflection::utils::cpp_default__load<::std::uint8_t>,
                      serialize_handler   = ::kryga::reflection::utils::cpp_default__save<::std::uint8_t>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<::std::uint8_t>);
KRG_ar_external_define(::std::uint8_t);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::std::uint16_t>,
                      load_derive_handler = ::kryga::reflection::utils::cpp_default__load<::std::uint16_t>,
                      serialize_handler   = ::kryga::reflection::utils::cpp_default__save<::std::uint16_t>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<::std::uint16_t>);
KRG_ar_external_define(::std::uint16_t);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::std::uint32_t>,
                      load_derive_handler = ::kryga::reflection::utils::cpp_default__load<::std::uint32_t>,
                      serialize_handler   = ::kryga::reflection::utils::cpp_default__save<::std::uint32_t>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<::std::uint32_t>);
KRG_ar_external_define(::std::uint32_t);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::std::uint64_t>,
                      load_derive_handler = ::kryga::reflection::utils::cpp_default__load<::std::uint64_t>,
                      serialize_handler   = ::kryga::reflection::utils::cpp_default__save<::std::uint64_t>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<::std::uint64_t>);
KRG_ar_external_define(::std::uint64_t);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<bool>,
                      load_derive_handler = ::kryga::reflection::utils::cpp_default__load<bool>,
                      serialize_handler   = ::kryga::reflection::utils::cpp_default__save<bool>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<bool>,
                      built_in            = true);
KRG_ar_external_define(bool);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<float>,
                      load_derive_handler = ::kryga::reflection::utils::cpp_default__load<float>,
                      serialize_handler   = ::kryga::reflection::utils::cpp_default__save<float>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<float>,
                      built_in            = true);
KRG_ar_external_define(float);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<double>,
                      load_derive_handler = ::kryga::reflection::utils::cpp_default__load<double>,
                      serialize_handler   = ::kryga::reflection::utils::cpp_default__save<double>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<double>,
                      built_in            = true);
KRG_ar_external_define(double);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::std::string>,
                      load_derive_handler = ::kryga::reflection::utils::cpp_default__load<::std::string>,
                      serialize_handler   = ::kryga::reflection::utils::cpp_default__save<::std::string>);
KRG_ar_external_define(::std::string);


KRG_ar_external_type(to_string_handler   = buffer__to_string,
                      copy_handler        = buffer__copy,
                      load_derive_handler = buffer__load,
                      serialize_handler   = buffer__save);
KRG_ar_external_define(::kryga::utils::buffer);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::kryga::utils::id>,
                      to_string_handler   = id__to_string,
                      load_derive_handler = id__load,
                      serialize_handler   = id__save);
KRG_ar_external_define(::kryga::utils::id);


//clang-format on