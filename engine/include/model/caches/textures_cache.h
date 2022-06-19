#pragma once

#include "core/agea_minimal.h"

#include "model/caches/base_cache.h"
#include "model/rendering/texture.h"

#include "utils/weird_singletone.h"

namespace agea
{
namespace glob
{
struct textures_cache : public simple_singleton<::agea::model::textures_cache*>
{
};

}  // namespace glob

}  // namespace agea