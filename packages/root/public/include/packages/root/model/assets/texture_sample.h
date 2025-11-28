#pragma once

#include "packages/root/model/assets/texture.h"

namespace agea::root
{
class texture;

// clang-format off
AGEA_ar_struct(copy_handler                  = texture_sample__copy,
              instantiate_handler            = texture_sample__instantiate,
              compare_handler                = texture_sample__compare,
              serialize_handler              = texture_sample__serialize,
              load_derive                    = texture_sample__load_derive,
              deserialize_handler            = texture_sample__deserialize);
struct texture_sample
{
    uint32_t slot = -1;
    utils::id sampler_id;
    texture* txt = nullptr;
};

}  // namespace agea::root
