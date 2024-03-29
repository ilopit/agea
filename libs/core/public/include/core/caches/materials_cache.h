#pragma once

#include "core/model_fwds.h"

#include <utils/singleton_instance.h>

namespace agea
{
namespace glob
{
struct materials_cache : public singleton_instance<::agea::core::materials_cache, materials_cache>
{
};

struct proto_materials_cache
    : public singleton_instance<::agea::core::materials_cache, proto_materials_cache>
{
};
}  // namespace glob
}  // namespace agea