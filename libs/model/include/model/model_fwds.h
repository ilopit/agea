#pragma once

#include <string>

#include "model/architype.h"
#include <functional>

namespace agea
{
namespace root
{
class asset;
class caches_map;
class cache_set;
class cache_set_ref;

class component;
class game_object;
class game_object_component;

class texture;
class mesh;
class material;
class smart_object;
class shader_effect;
class light_component;

}  // namespace root

namespace model
{
class class_objects_cache;
class object_load_context;
class level;
class package;
class package_manager;

template <typename T, architype ID>
class cache;

using objects_cache = cache<::agea::root::smart_object, architype::smart_object>;
using components_cache = cache<::agea::root::component, architype::component>;
using game_objects_cache = cache<::agea::root::game_object, architype::game_object>;
using textures_cache = cache<::agea::root::texture, architype::texture>;
using meshes_cache = cache<::agea::root::mesh, architype::mesh>;
using materials_cache = cache<::agea::root::material, architype::material>;
using shader_effects_cache = cache<::agea::root::shader_effect, architype::shader_effect>;

}  // namespace model
}  // namespace agea