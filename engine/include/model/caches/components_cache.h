#pragma once

#include "core/agea_minimal.h"

#include "model/caches/hash_cache.h"
#include "model/components/component.h"

#include "utils/weird_singletone.h"

namespace agea
{
namespace glob
{
struct components_cache : public simple_singleton<::agea::model::components_cache*>
{
};
struct class_components_cache : public simple_singleton<::agea::model::components_cache*>
{
};
}  // namespace glob
}  // namespace agea