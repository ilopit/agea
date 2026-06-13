#pragma once

#include <core/caches/cache_set.h>
#include <core/level.h>
#include <core/level_manager.h>
#include <core/package_manager.h>
#include <core/id_generator.h>
#include <core/model_output.h>
#include <core/reflection/reflection_type.h>

#include <global_state/system.h>

namespace kryga::core
{

class model_system : public gs::system
{
public:
    std::string_view
    name() const override
    {
        return "model";
    }
    std::span<const std::string_view>
    deps() const override
    {
        return {};
    }

    void
    on_init(gs::state&) override
    {
        packages.init();
    }

    cache_set caches;

    reflection::reflection_type_registry reflection;
    id_generator id_gen;
    level_manager levels;
    package_manager packages;

    level* current_level = nullptr;

    // The model's per-frame output to the render side (main thread only).
    // Formerly a standalone global service box (core::queues); folded in here
    // because it is model state and belongs to the model domain. The render
    // command queue is a separate render-side subsystem (render::input_queue).
    model_output output;
};

}  // namespace kryga::core
