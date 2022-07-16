#pragma once

#include "model/model_minimal.h"

#include "model/model_fwds.h"

#include "utils/weird_singletone.h"

namespace agea
{
namespace model
{
class cache_set_ref
{
public:
    objects_cache* objects = nullptr;
    components_cache* components = nullptr;
    game_objects_cache* game_objects = nullptr;
    materials_cache* materials = nullptr;
    meshes_cache* meshes = nullptr;
    textures_cache* textures = nullptr;

    caches_map* map = nullptr;
};

class cache_set
{
public:
    cache_set();
    ~cache_set();

    cache_set&
    operator=(cache_set&&) noexcept;
    cache_set(cache_set&&) noexcept;

    cache_set_ref
    get_ref() const;

    std::unique_ptr<objects_cache> objects;
    std::unique_ptr<components_cache> components;
    std::unique_ptr<game_objects_cache> game_objects;
    std::unique_ptr<materials_cache> materials;
    std::unique_ptr<meshes_cache> meshes;
    std::unique_ptr<textures_cache> textures;

    std::unique_ptr<caches_map> map;
};

}  // namespace model

namespace glob
{
struct class_objects_cache_set_view : public simple_singleton<::agea::model::cache_set_ref, 1>
{
};

struct objects_cache_set_view : public simple_singleton<::agea::model::cache_set_ref, 2>
{
};

struct class_objects_cache_set : public ::agea::selfcleanable_singleton<::agea::model::cache_set, 1>
{
};

struct objects_cache_set : public ::agea::selfcleanable_singleton<::agea::model::cache_set, 2>
{
};

void
init_global_caches(::agea::singletone_autodeleter& class_objects_cache_set,
                   ::agea::singletone_autodeleter& objects_cache_set);

}  // namespace glob

}  // namespace agea