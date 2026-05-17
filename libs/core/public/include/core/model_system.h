#pragma once

#include <core/caches/cache_set.h>
#include <core/level.h>
#include <core/level_manager.h>
#include <core/package_manager.h>
#include <core/id_generator.h>
#include <core/reflection/reflection_type.h>

#include <global_state/system.h>

namespace kryga::core
{

class model_system : public gs::system
{
public:
    std::string_view
    system_name() const override
    {
        return "model";
    }
    std::span<const std::string_view>
    system_deps() const override
    {
        return {};
    }

    void
    on_init(gs::state&) override
    {
        packages.init();
    }

    cache_set class_caches;
    cache_set instance_caches;

    level_manager levels;
    package_manager packages;
    id_generator id_gen;
    reflection::reflection_type_registry reflection;

    level* current_level = nullptr;
};

}  // namespace kryga::core
