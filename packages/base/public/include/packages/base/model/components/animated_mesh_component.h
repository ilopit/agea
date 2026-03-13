#pragma once

#include "packages/base/model/animated_mesh_component.ar.h"

#include "packages/root/model/components/game_object_component.h"

#include <utils/buffer.h>
#include <utils/id.h>

namespace kryga
{
namespace root
{
class material;
}  // namespace root

namespace base
{
// clang-format off
KRG_ar_class(render_cmd_builder   = animated_mesh_component__cmd_builder,
              render_cmd_destroyer = animated_mesh_component__cmd_destroyer);
class animated_mesh_component : public ::kryga::root::game_object_component
// clang-format on
{
    KRG_gen_meta__animated_mesh_component();

public:
    KRG_gen_class_meta(animated_mesh_component, ::kryga::root::game_object_component);

    KRG_gen_construct_params
    {
        utils::buffer gltf;
        ::kryga::root::material* material_handle = nullptr;
    };
    KRG_gen_meta_api;

    bool
    construct(construct_params& c);

    float
    get_base_bounding_radius() const
    {
        return m_base_bounding_radius;
    }
    void
    set_base_bounding_radius(float r)
    {
        m_base_bounding_radius = r;
    }

    const utils::id&
    get_animation_instance_id() const
    {
        return m_anim_instance_id;
    }
    void
    set_anim_instance_id(const utils::id& v)
    {
        m_anim_instance_id = v;
    }
    void
    set_skeleton_id(const utils::id& v)
    {
        m_skeleton_id = v;
    }

protected:
    KRG_ar_property("category=Assets",
                     "serializable=true",
                     "invalidates=render",
                     "access=all");
    utils::buffer m_gltf;

    KRG_ar_property("category=Assets",
                     "serializable=true",
                     "invalidates=render",
                     "access=all",
                     "default=true");
    ::kryga::root::material* m_material = nullptr;

    KRG_ar_property("category=Animation",
                     "serializable=true",
                     "access=all");
    utils::id m_clip_name;

    KRG_ar_property("category=Animation",
                     "serializable=true",
                     "access=all",
                     "default=true");
    float m_playback_speed = 1.0f;

    KRG_ar_property("category=Animation",
                     "serializable=true",
                     "access=all",
                     "default=true");
    bool m_looping = true;

    KRG_ar_property("category=Animation",
                     "serializable=true",
                     "access=all",
                     "default=true");
    bool m_playing = true;

    // Runtime state (not serialized)
    float m_base_bounding_radius = 0.0f;
    utils::id m_anim_instance_id;
    utils::id m_skeleton_id;
};

}  // namespace base
}  // namespace kryga
