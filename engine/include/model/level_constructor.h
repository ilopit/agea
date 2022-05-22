#pragma once

#include "core/agea_minimal.h"

#include <string>

namespace agea
{
namespace model
{
class level;
class mesh_component;
class game_object;

namespace level_constructor
{
bool
load_level_id(level& l, const std::string& id);
bool
load_level_path(level& l, const std::string& path);

bool
fill_level_caches(level& l);

}  // namespace level_constructor
}  // namespace model
}  // namespace agea
