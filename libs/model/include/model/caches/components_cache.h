#pragma once

#include "model/model_minimal.h"
#include "model/caches/hash_cache.h"

#include <utils/singleton_instance.h>

namespace agea
{
namespace glob
{
struct components_cache
    : public singleton_instance<::agea::model::components_cache, components_cache>
{
};
struct class_components_cache
    : public singleton_instance<::agea::model::components_cache, class_components_cache>
{
};
}  // namespace glob
}  // namespace agea