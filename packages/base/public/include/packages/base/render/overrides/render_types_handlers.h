#pragma once

#include <ar/ar_defines.h>

#include <error_handling/error_handling.h>
#include <core/reflection/reflection_type.h>

namespace kryga
{
class render_bridge;

namespace root
{
class smart_object;
}

KRG_ar_render_overrides();

result_code
mesh_component__cmd_builder(reflection::type_context__render_cmd_build& ctx);
result_code
mesh_component__cmd_destroyer(reflection::type_context__render_cmd_build& ctx);

result_code
directional_light_component__cmd_builder(reflection::type_context__render_cmd_build& ctx);
result_code
directional_light_component__cmd_destroyer(reflection::type_context__render_cmd_build& ctx);

result_code
spot_light_component__cmd_builder(reflection::type_context__render_cmd_build& ctx);
result_code
spot_light_component__cmd_destroyer(reflection::type_context__render_cmd_build& ctx);

result_code
point_light_component__cmd_builder(reflection::type_context__render_cmd_build& ctx);
result_code
point_light_component__cmd_destroyer(reflection::type_context__render_cmd_build& ctx);

result_code
animated_mesh_component__cmd_builder(reflection::type_context__render_cmd_build& ctx);
result_code
animated_mesh_component__cmd_destroyer(reflection::type_context__render_cmd_build& ctx);

}  // namespace kryga
