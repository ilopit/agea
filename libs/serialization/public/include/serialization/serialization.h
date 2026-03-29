#pragma once

#include "serialization/serialization_fwds.h"

#include <vfs/rid.h>
#include <utils/path.h>

#include <yaml-cpp/yaml.h>
#include <string>

namespace kryga
{
namespace serialization
{

bool
read_container(const utils::path& path, serialization::container& container);

bool
write_container(const utils::path& path, const serialization::container& container);

bool
read_container(const vfs::rid& id, serialization::container& container);

bool
write_container(const vfs::rid& id, const serialization::container& container);

}  // namespace serialization
}  // namespace kryga
