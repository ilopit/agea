#pragma once

#include "serialization/serialization_fwds.h"

#include <yaml-cpp/yaml.h>
#include <string>

#include "utils/path.h"

namespace agea
{
namespace serialization
{

bool
read_container(const utils::path& path, serialization::conteiner& conteiner);

bool
write_container(const utils::path& path, const serialization::conteiner& conteiner);

}  // namespace serialization
}  // namespace agea
