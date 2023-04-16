#pragma once

#include "core/model_minimal.h"
#include "core/caches/hash_cache.h"

#include <utils/singleton_instance.h>

namespace agea
{
namespace glob
{
struct components_cache
    : public singleton_instance<::agea::core::components_cache, components_cache>
{
};
struct class_components_cache
    : public singleton_instance<::agea::core::components_cache, class_components_cache>
{
};
}  // namespace glob
}  // namespace agea