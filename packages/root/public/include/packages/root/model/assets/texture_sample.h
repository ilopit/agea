#pragma once

#include "packages/root/model/assets/texture.h"

namespace agea::root
{
class texture;

AGEA_ar_struct();
struct texture_sample
{
    uint32_t slot = -1;
    utils::id sampler_id;
    texture* txt = nullptr;
};

}  // namespace agea::root
