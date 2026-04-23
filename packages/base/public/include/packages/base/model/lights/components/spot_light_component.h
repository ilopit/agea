#pragma once

#include "packages/base/model/spot_light_component.ar.h"

#include "packages/root/model/components/game_object_component.h"

#include <utils/slot_handle.h>

namespace kryga
{
namespace render
{
class vulkan_universal_light_data;
}

namespace base
{

KRG_ar_class(render_cmd_builder = spot_light_component__cmd_builder,
             render_cmd_destroyer = spot_light_component__cmd_destroyer);
class spot_light_component : public ::kryga::root::game_object_component
{
    KRG_gen_meta__spot_light_component();

public:
    KRG_gen_class_meta(spot_light_component, game_object_component);

    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    utils::slot_handle<render::vulkan_universal_light_data>
    get_render_light_handle() const
    {
        return m_render_light_handle;
    }

    void
    set_render_light_handle(utils::slot_handle<render::vulkan_universal_light_data> h)
    {
        m_render_light_handle = h;
    }

protected:
    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    ::kryga::root::vec3 m_ambient = glm::vec3{1.0f};

    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    ::kryga::root::vec3 m_diffuse = glm::vec3{1.0f};

    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    ::kryga::root::vec3 m_direction = glm::vec3{1.0f};

    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    ::kryga::root::vec3 m_specular = glm::vec3{1.0f};

    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    float m_radius = 50.0f;

    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    float m_cut_off = 17.0f;

    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    float m_outer_cut_off = 18.f;

    utils::slot_handle<render::vulkan_universal_light_data> m_render_light_handle;
};

}  // namespace base
}  // namespace kryga
