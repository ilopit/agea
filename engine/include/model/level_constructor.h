#pragma once

#include "core/agea_minimal.h"
#include "model/caches/cache_set.h"

#include <string>

#include "model/model_fwds.h"

namespace agea
{
namespace model
{

namespace level_constructor
{
bool
load_level_id(level& l, const std::string& id, cache_set_ref global_cs);
bool
load_level_path(level& l, const utils::path& path, cache_set_ref global_cs);
bool
save_level(level& l, const utils::path& path);

}  // namespace level_constructor
}  // namespace model
}  // namespace agea
