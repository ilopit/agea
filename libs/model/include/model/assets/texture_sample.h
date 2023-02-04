#pragma once

#include "model/assets/texture.h"

namespace agea
{
namespace model
{
class texture;

struct texture_sample
{
    uint32_t slot = -1;
    utils::id sampler_id;
    texture* txt = nullptr;
};

}  // namespace model
}  // namespace agea