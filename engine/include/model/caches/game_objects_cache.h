#pragma once

#include "core/agea_minimal.h"

#include "model/caches/base_cache.h"
#include "model/game_object.h"

#include "utils/weird_singletone.h"

namespace agea
{
namespace glob
{
struct game_objects_cache : public weird_singleton<::agea::model::game_objects_cache>
{
};

}  // namespace glob

}  // namespace agea