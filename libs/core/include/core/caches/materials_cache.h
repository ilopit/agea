#pragma once

#include "core/model_fwds.h"
#include "core/caches/hash_cache.h"

#include <utils/singleton_instance.h>

namespace agea
{
namespace glob
{
struct materials_cache : public singleton_instance<::agea::core::materials_cache, materials_cache>
{
};

struct class_materials_cache
    : public singleton_instance<::agea::core::materials_cache, class_materials_cache>
{
};
}  // namespace glob
}  // namespace agea