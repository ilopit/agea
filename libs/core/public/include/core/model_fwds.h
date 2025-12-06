#pragma once

#include "core/architype.h"

#include <functional>
#include <string>

namespace agea
{
namespace root
{
class asset;

class component;
class game_object;
class game_object_component;
class mesh_component;

class texture;
class mesh;
class material;
class smart_object;
class shader_effect;
class light_component;
class point_light;

}  // namespace root

namespace core
{
class caches_map;
class cache_set;
class cache_set_ref;
class class_objects_cache;
class object_load_context;
class level;
class package;
class static_package;
class package_manager;
class level_manager;
class object_mapping;
class id_generator;

struct objects_cache;
struct components_cache;
struct game_objects_cache;
struct textures_cache;
struct meshes_cache;
struct materials_cache;
struct shader_effects_cache;

}  // namespace core
}  // namespace agea