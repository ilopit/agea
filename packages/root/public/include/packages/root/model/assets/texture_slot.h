#pragma once

#include "packages/root/model/assets/texture.h"

namespace kryga::root
{
class texture;
class sampler;

// clang-format off
KRG_ar_struct(copy_handler                  = texture_slot__copy,
              instantiate_handler            = texture_slot__instantiate,
              compare_handler                = texture_slot__compare,
              serialize_handler              = texture_slot__save,
              load_derive_handler            = texture_slot__load,
              deserialize_handler            = texture_slot__deserialize,
              json_save_handler              = texture_slot__json_save,
              json_load_handler              = texture_slot__json_load);
struct texture_slot
{
    uint32_t slot = 0;
    sampler* smp = nullptr;
    texture* txt = nullptr;
};

}  // namespace kryga::root
