#pragma once

#include "core/agea_minimal.h"

#include "model/caches/hash_cache.h"
#include "model/game_object.h"

#include "utils/weird_singletone.h"

namespace agea
{
namespace glob
{
struct objects_cache : public simple_singleton<::agea::model::objects_cache*>
{
};

struct class_objects_cache : public simple_singleton<::agea::model::objects_cache*>
{
};

}  // namespace glob

}  // namespace agea