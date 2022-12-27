#pragma once

#include "model/model_fwds.h"
#include "model/caches/hash_cache.h"

#include <utils/singleton_instance.h>

namespace agea
{
namespace glob
{
struct game_objects_cache
    : public singleton_instance<::agea::model::game_objects_cache, game_objects_cache>
{
};

struct class_game_objects_cache
    : public singleton_instance<::agea::model::game_objects_cache, class_game_objects_cache>
{
};

}  // namespace glob

}  // namespace agea