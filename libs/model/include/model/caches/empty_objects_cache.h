#pragma once

#include "model/model_minimal.h"
#include "model/caches/hash_cache.h"

#include <root/smart_object.h>

#include <utils/singleton_instance.h>

namespace agea
{
namespace glob
{
struct empty_objects_cache
    : public singleton_instance<::agea::model::hash_cache<utils::id>, empty_objects_cache>
{
};

}  // namespace glob
}  // namespace agea