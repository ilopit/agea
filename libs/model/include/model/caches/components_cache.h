#pragma once

#include "model/model_minimal.h"

#include "model/caches/hash_cache.h"
#include "model/components/component.h"

#include "utils/weird_singletone.h"

namespace agea
{
namespace glob
{
struct components_cache : public simple_singleton<::agea::model::components_cache*, 1>
{
};
struct class_components_cache : public simple_singleton<::agea::model::components_cache*, 2>
{
};
}  // namespace glob
}  // namespace agea