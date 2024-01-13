#pragma once

#include "packages/root/assets/texture.h"

namespace agea
{
namespace root
{
class texture;

struct texture_sample
{
    uint32_t slot = -1;
    utils::id sampler_id;
    texture* txt = nullptr;
};

}  // namespace root
}  // namespace agea