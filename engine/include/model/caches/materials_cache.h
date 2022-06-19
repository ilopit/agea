#pragma once

#include "core/agea_minimal.h"
#include "utils/weird_singletone.h"
#include "model/model_fwds.h"
#include "model/caches/base_cache.h"

#include "model/rendering/material.h"

namespace agea
{
namespace glob
{
struct materials_cache : public simple_singleton<::agea::model::materials_cache*>
{
};

}  // namespace glob
}  // namespace agea