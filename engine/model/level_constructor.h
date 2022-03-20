#pragma once

#include "agea_minimal.h"

#include <string>

namespace agea
{
namespace model
{
class level;
class mesh_component;
class level_object;

namespace level_constructor
{
bool load_level(level& l, const std::string& id);
}  // namespace level_constructor
}  // namespace model
}  // namespace agea
