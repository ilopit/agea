#pragma once

#include "core/agea_minimal.h"
#include "model/caches/cache_set.h"

#include <string>

#include "model/model_fwds.h"

namespace agea
{
namespace model
{
class level_constructor
{
public:
    static bool
    load_level_id(level& l, const core::id& id, cache_set_ref global_cs);
    static bool
    load_level_path(level& l, const utils::path& path, cache_set_ref global_cs);
    static bool
    save_level(level& l, const utils::path& path);
};
}  // namespace model
}  // namespace agea
