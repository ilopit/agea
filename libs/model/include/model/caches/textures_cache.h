#pragma once

#include "model/model_minimal.h"

#include "model/caches/hash_cache.h"
#include "model/assets/texture.h"

#include "utils/weird_singletone.h"

namespace agea
{
namespace glob
{
struct textures_cache : public simple_singleton<::agea::model::textures_cache*, 1>
{
};
struct class_textures_cache : public simple_singleton<::agea::model::textures_cache*, 2>
{
};

}  // namespace glob
}  // namespace agea