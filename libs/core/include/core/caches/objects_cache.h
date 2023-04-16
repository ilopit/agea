#pragma once

#include "core/model_fwds.h"
#include "core/caches/hash_cache.h"

#include <utils/singleton_instance.h>

namespace agea
{
namespace glob
{
struct objects_cache : public singleton_instance<::agea::core::objects_cache, objects_cache>
{
};

struct class_objects_cache
    : public singleton_instance<::agea::core::objects_cache, class_objects_cache>
{
};

}  // namespace glob

}  // namespace agea