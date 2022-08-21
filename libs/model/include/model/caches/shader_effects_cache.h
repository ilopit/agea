#pragma once

#include "model/model_minimal.h"

#include "model/caches/hash_cache.h"
#include "model/assets/shader_effect.h"

#include "utils/weird_singletone.h"

namespace agea
{
namespace glob
{
struct shader_effects_cache : public simple_singleton<::agea::model::shader_effects_cache*, 1>
{
};
struct class_shader_effects_cache : public simple_singleton<::agea::model::shader_effects_cache*, 2>
{
};

}  // namespace glob
}  // namespace agea