#pragma once

#include "core/model_fwds.h"

#include "core/model_minimal.h"
#include "core/caches/cache_set.h"

namespace agea::core
{
class level_manager
{
public:
    level*
    load_level(const utils::id& id);

    void
    unload_level(level& l);

    static bool
    save_level(level& l, const utils::path& path);

private:
    level*
    load_level_path(level& l, const utils::path& path);

    std::unordered_map<utils::id, std::unique_ptr<level>> m_levels;
};
}  // namespace agea::core
