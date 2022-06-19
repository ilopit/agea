#pragma once

#include "core/agea_minimal.h"

#include "model/rendering/mesh.h"
#include "model/caches/base_cache.h"
#include "model/model_fwds.h"

#include "utils/weird_singletone.h"

namespace agea
{
namespace glob
{
struct meshes_cache : public simple_singleton<::agea::model::meshes_cache*>
{
};
}  // namespace glob
}  // namespace agea