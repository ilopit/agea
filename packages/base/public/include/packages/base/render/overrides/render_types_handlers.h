#pragma once

#include <ar/ar_defines.h>

#include <error_handling/error_handling.h>

namespace agea
{
class render_bridge;

namespace root
{
class smart_object;
}

AGEA_ar_render_overrides();

result_code
mesh_component__render_loader(::agea::render_bridge& rb, root::smart_object& obj, bool sub_object);

result_code
mesh_component__render_destructor(::agea::render_bridge& rb,
                                  root::smart_object& obj,
                                  bool sub_object);

result_code
directional_light_component__render_loader(::agea::render_bridge& rb,
                                           root::smart_object& obj,
                                           bool sub_object);
result_code
directional_light_component__render_destructor(::agea::render_bridge& rb,
                                               root::smart_object& obj,
                                               bool sub_object);

result_code
spot_light_component__render_loader(::agea::render_bridge& rb,
                                    root::smart_object& obj,
                                    bool sub_object);
result_code
spot_light_component__render_destructor(::agea::render_bridge& rb,
                                        root::smart_object& obj,
                                        bool sub_object);

result_code
point_light_component__render_loader(::agea::render_bridge& rb,
                                     root::smart_object& obj,
                                     bool sub_object);

result_code
point_light_component__render_destructor(::agea::render_bridge& rb,
                                         root::smart_object& obj,
                                         bool sub_object);

}  // namespace agea