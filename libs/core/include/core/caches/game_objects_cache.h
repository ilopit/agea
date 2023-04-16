#pragma once

#include "core/model_fwds.h"
#include "core/caches/hash_cache.h"

#include <utils/singleton_instance.h>

namespace agea
{
namespace glob
{
struct game_objects_cache
    : public singleton_instance<::agea::core::game_objects_cache, game_objects_cache>
{
};

struct class_game_objects_cache
    : public singleton_instance<::agea::core::game_objects_cache, class_game_objects_cache>
{
};

}  // namespace glob

}  // namespace agea