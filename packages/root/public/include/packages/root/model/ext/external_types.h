#pragma once

#include <cstdint>
#include <ar/ar_defines.h>

#include <utils/id.h>
#include <utils/buffer.h>
#include <core/object_layer_flags.h>

// clang-format off

KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::std::int8_t>,
                      load_handler = ::kryga::reflection::utils::cpp_default__load<::std::int8_t>,
                      save_handler   = ::kryga::reflection::utils::cpp_default__save<::std::int8_t>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<::std::int8_t>,
                      json_save_handler   = ::kryga::root::int8__json_save,
                      json_load_handler   = ::kryga::root::int8__json_load);
KRG_ar_external_define(::std::int8_t);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::std::int16_t>,
                      load_handler = ::kryga::reflection::utils::cpp_default__load<::std::int16_t>,
                      save_handler   = ::kryga::reflection::utils::cpp_default__save<::std::int16_t>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<::std::int16_t>,
                      json_save_handler   = ::kryga::root::int16__json_save,
                      json_load_handler   = ::kryga::root::int16__json_load);
KRG_ar_external_define(::std::int16_t);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::std::int32_t>,
                      load_handler = ::kryga::reflection::utils::cpp_default__load<::std::int32_t>,
                      save_handler   = ::kryga::reflection::utils::cpp_default__save<::std::int32_t>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<::std::int32_t>,
                      json_save_handler   = ::kryga::root::int32__json_save,
                      json_load_handler   = ::kryga::root::int32__json_load);
KRG_ar_external_define(::std::int32_t);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::std::int64_t>,
                      load_handler = ::kryga::reflection::utils::cpp_default__load<::std::int64_t>,
                      save_handler   = ::kryga::reflection::utils::cpp_default__save<::std::int64_t>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<::std::int64_t>,
                      json_save_handler   = ::kryga::root::int64__json_save,
                      json_load_handler   = ::kryga::root::int64__json_load);
KRG_ar_external_define(::std::int64_t);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::std::uint8_t>,
                      load_handler = ::kryga::reflection::utils::cpp_default__load<::std::uint8_t>,
                      save_handler   = ::kryga::reflection::utils::cpp_default__save<::std::uint8_t>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<::std::uint8_t>,
                      json_save_handler   = ::kryga::root::uint8__json_save,
                      json_load_handler   = ::kryga::root::uint8__json_load);
KRG_ar_external_define(::std::uint8_t);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::std::uint16_t>,
                      load_handler = ::kryga::reflection::utils::cpp_default__load<::std::uint16_t>,
                      save_handler   = ::kryga::reflection::utils::cpp_default__save<::std::uint16_t>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<::std::uint16_t>,
                      json_save_handler   = ::kryga::root::uint16__json_save,
                      json_load_handler   = ::kryga::root::uint16__json_load);
KRG_ar_external_define(::std::uint16_t);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::std::uint32_t>,
                      load_handler = ::kryga::reflection::utils::cpp_default__load<::std::uint32_t>,
                      save_handler   = ::kryga::reflection::utils::cpp_default__save<::std::uint32_t>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<::std::uint32_t>,
                      json_save_handler   = ::kryga::root::uint32__json_save,
                      json_load_handler   = ::kryga::root::uint32__json_load);
KRG_ar_external_define(::std::uint32_t);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::std::uint64_t>,
                      load_handler = ::kryga::reflection::utils::cpp_default__load<::std::uint64_t>,
                      save_handler   = ::kryga::reflection::utils::cpp_default__save<::std::uint64_t>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<::std::uint64_t>,
                      json_save_handler   = ::kryga::root::uint64__json_save,
                      json_load_handler   = ::kryga::root::uint64__json_load);
KRG_ar_external_define(::std::uint64_t);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<bool>,
                      load_handler = ::kryga::reflection::utils::cpp_default__load<bool>,
                      save_handler   = ::kryga::reflection::utils::cpp_default__save<bool>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<bool>,
                      json_save_handler   = ::kryga::root::bool__json_save,
                      json_load_handler   = ::kryga::root::bool__json_load,
                      built_in            = true);
KRG_ar_external_define(bool);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<float>,
                      load_handler = ::kryga::reflection::utils::cpp_default__load<float>,
                      save_handler   = ::kryga::reflection::utils::cpp_default__save<float>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<float>,
                      json_save_handler   = ::kryga::root::float__json_save,
                      json_load_handler   = ::kryga::root::float__json_load,
                      built_in            = true);
KRG_ar_external_define(float);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<double>,
                      load_handler = ::kryga::reflection::utils::cpp_default__load<double>,
                      save_handler   = ::kryga::reflection::utils::cpp_default__save<double>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<double>,
                      json_save_handler   = ::kryga::root::double__json_save,
                      json_load_handler   = ::kryga::root::double__json_load,
                      built_in            = true);
KRG_ar_external_define(double);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::std::string>,
                      load_handler = ::kryga::reflection::utils::cpp_default__load<::std::string>,
                      save_handler   = ::kryga::reflection::utils::cpp_default__save<::std::string>,
                      json_save_handler   = ::kryga::root::string__json_save,
                      json_load_handler   = ::kryga::root::string__json_load);
KRG_ar_external_define(::std::string);


KRG_ar_external_type(copy_handler        = buffer__copy,
                      load_handler = buffer__load,
                      save_handler   = buffer__save,
                      compare_handler     = buffer__compare);
KRG_ar_external_define(::kryga::utils::buffer);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::kryga::utils::id>,
                      load_handler = id__load,
                      save_handler   = id__save,
                      json_save_handler   = ::kryga::root::id__json_save,
                      json_load_handler   = ::kryga::root::id__json_load);
KRG_ar_external_define(::kryga::utils::id);


KRG_ar_external_type(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::kryga::core::object_layer_flags>,
                      load_handler = ::kryga::reflection::utils::cpp_default__load<::kryga::core::object_layer_flags>,
                      save_handler   = ::kryga::reflection::utils::cpp_default__save<::kryga::core::object_layer_flags>,
                      compare_handler     = ::kryga::reflection::utils::cpp_default__compare<::kryga::core::object_layer_flags>);
KRG_ar_external_define(::kryga::core::object_layer_flags);

//clang-format on