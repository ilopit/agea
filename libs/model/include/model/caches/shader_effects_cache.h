#pragma once

#include "model/model_fwds.h"
#include "model/caches/hash_cache.h"

#include <utils/singleton_instance.h>

namespace agea
{
namespace glob
{
struct shader_effects_cache
    : public singleton_instance<::agea::model::shader_effects_cache, shader_effects_cache>
{
};
struct class_shader_effects_cache
    : public singleton_instance<::agea::model::shader_effects_cache, class_shader_effects_cache>
{
};

}  // namespace glob
}  // namespace agea