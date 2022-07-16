#pragma once

#include "model/model_minimal.h"

#include "model/caches/hash_cache.h"
#include "model/game_object.h"

#include <utils/weird_singletone.h>

namespace agea
{
namespace glob
{
struct game_objects_cache : public selfcleanable_singleton<::agea::model::game_objects_cache>
{
};

}  // namespace glob

}  // namespace agea