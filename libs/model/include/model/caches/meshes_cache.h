#pragma once

#include "model/model_minimal.h"

#include "model/assets/mesh.h"
#include "model/caches/hash_cache.h"
#include "model/model_fwds.h"

#include "utils/weird_singletone.h"

namespace agea
{
namespace glob
{
struct meshes_cache : public simple_singleton<::agea::model::meshes_cache*, 1>
{
};

struct class_meshes_cache : public simple_singleton<::agea::model::meshes_cache*, 2>
{
};

}  // namespace glob
}  // namespace agea