#pragma once

#include "serialization/serialization_fwds.h"

#include <utils/path.h>

#include <yaml-cpp/yaml.h>
#include <string>

namespace agea
{
namespace serialization
{

bool
read_container(const utils::path& path, serialization::container& container);

bool
write_container(const utils::path& path, const serialization::container& container);

}  // namespace serialization
}  // namespace agea
