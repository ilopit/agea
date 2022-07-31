#pragma once

#include "model/caches/hash_cache.h"
#include "model/game_object.h"

#include "utils/weird_singletone.h"

namespace agea
{
namespace glob
{
struct objects_cache : public simple_singleton<::agea::model::objects_cache*, 1>
{
};

struct class_objects_cache : public simple_singleton<::agea::model::objects_cache*, 2>
{
};

}  // namespace glob

}  // namespace agea