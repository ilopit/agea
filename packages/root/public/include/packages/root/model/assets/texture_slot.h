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
              save_handler                   = texture_slot__save,
              load_handler                   = texture_slot__load,
              json_save_handler              = texture_slot__json_save,
              json_load_handler              = texture_slot__json_load,
              mcp_schema                     = "string:asset_id",
              mcp_hint                       = "Texture + sampler pair bound to a material slot — set the texture ID to change the image");
struct texture_slot
{
    uint32_t slot = 0;
    sampler* smp = nullptr;
    texture* txt = nullptr;
};

}  // namespace kryga::root
