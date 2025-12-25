#pragma once

#include <ar/ar_defines.h>

#include <error_handling/error_handling.h>
#include <core/reflection/reflection_type.h>

namespace agea
{
class render_bridge;

namespace root
{
class smart_object;
}

AGEA_ar_render_overrides();

result_code
mesh_component__render_loader(reflection::type_context__render& ctx);

result_code
mesh_component__render_destructor(reflection::type_context__render& ctx);

result_code
directional_light_component__render_loader(reflection::type_context__render& ctx);
result_code
directional_light_component__render_destructor(reflection::type_context__render& ctx);

result_code
spot_light_component__render_loader(reflection::type_context__render& ctx);
result_code
spot_light_component__render_destructor(reflection::type_context__render& ctx);

result_code
point_light_component__render_loader(reflection::type_context__render& ctx);

result_code
point_light_component__render_destructor(reflection::type_context__render& ctx);

}  // namespace agea
