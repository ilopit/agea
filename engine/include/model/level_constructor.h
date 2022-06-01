#pragma once

#include "core/agea_minimal.h"

#include <string>

#include "model/model_fwds.h"

namespace agea
{
namespace model
{

namespace level_constructor
{
bool
load_level_id(level& l, const std::string& id);
bool
load_level_path(level& l, const std::string& path);
bool
save_level(level& l);

bool
fill_level_caches(level& l);

}  // namespace level_constructor
}  // namespace model
}  // namespace agea
