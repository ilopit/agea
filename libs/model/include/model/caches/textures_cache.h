#pragma once

#include "model/model_minimal.h"

#include "model/caches/hash_cache.h"

#include <utils/singleton_instance.h>

namespace agea
{
namespace glob
{
struct textures_cache : public singleton_instance<::agea::model::textures_cache, textures_cache>
{
};
struct class_textures_cache
    : public singleton_instance<::agea::model::textures_cache, class_textures_cache>
{
};

}  // namespace glob
}  // namespace agea