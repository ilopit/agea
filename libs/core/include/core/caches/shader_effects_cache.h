#pragma once

#include "core/model_fwds.h"
#include "core/caches/hash_cache.h"

#include <utils/singleton_instance.h>

namespace agea
{
namespace glob
{
struct shader_effects_cache
    : public singleton_instance<::agea::core::shader_effects_cache, shader_effects_cache>
{
};
struct class_shader_effects_cache
    : public singleton_instance<::agea::core::shader_effects_cache, class_shader_effects_cache>
{
};

}  // namespace glob
}  // namespace agea