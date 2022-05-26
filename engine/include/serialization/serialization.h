#pragma once

#include "serialization/serialization_fwds.h"

#include <yaml-cpp/yaml.h>

#include <string>

namespace agea
{
namespace serialization
{

bool
read_container(const std::string& path, serialization::conteiner& conteiner);
}  // namespace serialization
}  // namespace agea
