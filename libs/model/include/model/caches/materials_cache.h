#pragma once

#include "model/model_minimal.h"
#include "utils/weird_singletone.h"
#include "model/model_fwds.h"
#include "model/caches/hash_cache.h"

#include "model/assets/material.h"

namespace agea
{
namespace glob
{
struct materials_cache : public simple_singleton<::agea::model::materials_cache*, 1>
{
};

struct class_materials_cache : public simple_singleton<::agea::model::materials_cache*, 2>
{
};
}  // namespace glob
}  // namespace agea