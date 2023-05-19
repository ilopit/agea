#pragma once

#include "core/model_fwds.h"

#include "core/model_minimal.h"
#include "core/caches/cache_set.h"

namespace agea
{
namespace core
{
class level_manager
{
public:
    bool
    load_level_id(level& l,
                  const utils::id& id,
                  cache_set* global_class_cs,
                  cache_set* global_instances_cs);
    bool
    load_level_path(level& l,
                    const utils::path& path,
                    cache_set* global_class_cs,
                    cache_set* global_instances_cs);

    void
    unload_level(level& l);

    static bool
    save_level(level& l, const utils::path& path);

    std::unordered_map<utils::id, std::unique_ptr<level>> m_packages;
};
}  // namespace core

namespace glob
{
struct level_manager : public ::agea::singleton_instance<::agea::core::level_manager, level_manager>
{
};
}  // namespace glob
}  // namespace agea
