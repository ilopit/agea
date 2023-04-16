#pragma once

#include "core/model_fwds.h"
#include "core/caches/hash_cache.h"

#include <utils/singleton_instance.h>

namespace agea
{
namespace glob
{
struct textures_cache : public singleton_instance<::agea::core::textures_cache, textures_cache>
{
};
struct class_textures_cache
    : public singleton_instance<::agea::core::textures_cache, class_textures_cache>
{
};

}  // namespace glob
}  // namespace agea