#pragma once

#include "core/caches/cache_set.h"

#include "core/caches/components_cache.h"
#include "core/caches/game_objects_cache.h"
#include "core/caches/materials_cache.h"
#include "core/caches/meshes_cache.h"
#include "core/caches/textures_cache.h"
#include "core/caches/objects_cache.h"
#include "core/caches/shader_effects_cache.h"

#include "core/caches/caches_map.h"

#include "core/model_fwds.h"

namespace agea
{

glob::proto_objects_cache_set::type glob::proto_objects_cache_set::type::s_instance;
glob::objects_cache_set::type glob::objects_cache_set::type::s_instance;

namespace core
{
cache_set::cache_set()
    : objects(std::make_unique<objects_cache>())
    , components(std::make_unique<components_cache>())
    , game_objects(std::make_unique<game_objects_cache>())
    , materials(std::make_unique<materials_cache>())
    , meshes(std::make_unique<meshes_cache>())
    , textures(std::make_unique<textures_cache>())
    , shader_effects(std::make_unique<shader_effects_cache>())
    , map(std::make_unique<caches_map>())
{
    map->add_cache(objects.get());
    map->add_cache(components.get());
    map->add_cache(game_objects.get());
    map->add_cache(materials.get());
    map->add_cache(meshes.get());
    map->add_cache(textures.get());
    map->add_cache(shader_effects.get());
}

cache_set::cache_set(cache_set&&) noexcept = default;

cache_set&
cache_set::operator=(cache_set&&) noexcept = default;

cache_set::~cache_set() = default;

}  // namespace core

void
glob::init_global_caches(singleton_registry& r)
{
    // clang-format off
    glob::proto_objects_cache_set::create(r);
    glob::proto_object_caches_map::create_ref(glob::proto_objects_cache_set::get()->map.get());

    glob::proto_materials_cache::create_ref(glob::proto_objects_cache_set::getr().materials.get());
    glob::proto_meshes_cache::create_ref(glob::proto_objects_cache_set::getr().meshes.get());
    glob::proto_textures_cache::create_ref(glob::proto_objects_cache_set::getr().textures.get());
    glob::proto_objects_cache::create_ref(glob::proto_objects_cache_set::getr().objects.get());
    glob::proto_components_cache::create_ref(glob::proto_objects_cache_set::getr().components.get());
    glob::proto_shader_effects_cache::create_ref(glob::proto_objects_cache_set::getr().shader_effects.get());

    glob::objects_cache_set::create(r);
    glob::object_caches_map::create_ref(glob::objects_cache_set::get()->map.get());

    glob::materials_cache::create_ref(glob::objects_cache_set::getr().materials.get());
    glob::meshes_cache::create_ref(glob::objects_cache_set::getr().meshes.get());
    glob::textures_cache::create_ref(glob::objects_cache_set::getr().textures.get());
    glob::objects_cache::create_ref(glob::objects_cache_set::getr().objects.get());
    glob::components_cache::create_ref(glob::objects_cache_set::getr().components.get());
    glob::shader_effects_cache::create_ref(glob::objects_cache_set::getr().shader_effects.get());
    // clang-format on
}

}  // namespace agea