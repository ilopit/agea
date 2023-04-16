#pragma once

#include "core/model_minimal.h"
#include "core/caches/hash_cache.h"

#include <root/smart_object.h>

#include <utils/singleton_instance.h>

namespace agea
{
namespace glob
{
struct empty_objects_cache
    : public singleton_instance<::agea::core::hash_cache<utils::id>, empty_objects_cache>
{
};

}  // namespace glob
}  // namespace agea