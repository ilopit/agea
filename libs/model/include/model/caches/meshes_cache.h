#pragma once

#include "model/model_fwds.h"
#include "model/caches/hash_cache.h"

#include <utils/singleton_instance.h>

namespace agea
{
namespace glob
{
struct meshes_cache : public singleton_instance<::agea::model::meshes_cache, meshes_cache>
{
};

struct class_meshes_cache
    : public singleton_instance<::agea::model::meshes_cache, class_meshes_cache>
{
};

}  // namespace glob
}  // namespace agea