#pragma once

#include "packages/root/model/game_object.ar.h"

#include "packages/root/model/core_types/vec3.h"
#include "packages/root/model/components/game_object_component.h"
#include "packages/root/model/smart_object.h"

#include <core/model_minimal.h>
#include <core/model_system.h>
#include <core/id_generator.h>
#include <global_state/global_state.h>

namespace kryga
{
namespace root
{
class component;

// clang-format off
KRG_ar_class(
    "architype=game_object",
    mcp_hint = "Scene entity that owns components — a container whose behavior lives in its child "
               "components"
);
class game_object : public smart_object
// clang-format on
{
    KRG_gen_meta__game_object();

public:
    // Meta part
    KRG_gen_class_meta(game_object, smart_object);
    KRG_gen_construct_params
    {
        vec3 pos = {0.f};
    };
    KRG_gen_meta_api;

    bool
    construct(construct_params& params);

    bool
    construct_default(construct_params& params);

    virtual bool
    post_construct() override;

    virtual bool
    post_load() override;

    void
    update_position();

    void
    attach(component* c);

    template <typename T>
    T*
    spawn_component(component* parent,
                    const utils::id& id,
                    const typename T::construct_params& params)
    {
        auto gid = glob::glob_state().getr_model().id_gen.generate(id);

        auto* c = spawn_component(parent, T::AR_TYPE_id(), gid, params);
        return c ? c->template as<T>() : nullptr;
    }

    component*
    spawn_component(component* parent,
                    const utils::id& type_id,
                    const utils::id& id,
                    const root::component::construct_params& params);
    component*
    spawn_component_with_proto(component* parent, const utils::id& proto_id, const utils::id& id);

    component*
    spawn_component_with_proto(component* parent, const utils::id& proto_id);

    virtual void
    begin_play()
    {
    }

    virtual void
    end_play()
    {
    }

    virtual void
    on_tick(float)
    {
    }

    component*
    get_component_at(size_t idx) const
    {
        KRG_check(idx < m_components.size(), "Index should be in range");
        return m_components[idx];
    }

    // clang-format off
    KRG_ar_function(
        category = "world",
        mcp_hint = "Returns rotation as quaternion [x y z w]"
    );
    glm::quat
    get_rotation_quat() const
    // clang-format on
    {
        return m_root_component->get_rotation();
    }

    // clang-format off
    KRG_ar_function(
        category = "world",
        mcp_hint = "Returns local forward direction as unit [x y z]"
    );
    vec3
    get_forward_vector() const
    // clang-format on
    {
        return m_root_component->get_forward_vector();
    }

    // clang-format off
    KRG_ar_function(
        category = "world",
        mcp_hint = "Returns local up direction as unit [x y z]"
    );
    vec3
    get_up_vector() const
    // clang-format on
    {
        return m_root_component->get_up_vector();
    }

    // clang-format off
    KRG_ar_function(
        category = "world",
        mcp_hint = "Returns local right direction as unit [x y z]"
    );
    vec3
    get_right_vector() const
    // clang-format on
    {
        return m_root_component->get_right_vector();
    }

    // clang-format off
    KRG_ar_function(
        category = "world",
        mcp_hint = "Returns world position [x y z]"
    );
    vec3
    get_position() const
    // clang-format on
    {
        return m_root_component->get_position();
    }

    // clang-format off
    KRG_ar_function(
        category = "world",
        mcp_hint = "Sets world position [x y z]"
    );
    void
    set_position(vec3 v)
    // clang-format on
    {
        m_root_component->set_position(v);
    }

    // clang-format off
    KRG_ar_function(
        category = "world",
        mcp_hint = "Returns euler rotation in degrees [x y z]"
    );
    vec3
    get_rotation() const
    // clang-format on
    {
        return m_root_component->get_rotation();
    }

    // clang-format off
    KRG_ar_function(
        category = "world",
        mcp_hint = "Sets euler rotation in degrees [x y z]"
    );
    void
    set_rotation(vec3 v);
    // clang-format on

    // clang-format off
    KRG_ar_function(
        category = "world",
        mcp_hint = "Returns scale [x y z]"
    );
    vec3
    get_scale() const
    // clang-format on
    {
        return m_root_component->get_scale();
    }

    // clang-format off
    KRG_ar_function(
        category = "world",
        mcp_hint = "Sets scale [x y z]"
    );
    void
    set_scale(vec3 v)
    // clang-format on
    {
        m_root_component->set_scale(v);
    }

    void
    set_root_component(game_object_component* root)
    {
        KRG_check(!get_flags().readonly, "set_root_component: writing to readonly object");
        m_root_component = root;
    }

    game_object_component*
    get_root_component()
    {
        return m_root_component;
    }

    std::vector<game_object_component*>&
    get_renderable_components()
    {
        return m_renderable_components;
    }

    void
    recreate_structure_from_layout();

    void
    recreate_structure_from_ids();

    Range<component>
    get_components()
    {
        return {m_components.begin(), m_components.end()};
    }

    Range<component>
    get_components(uint32_t idx)
    {
        return {m_components.begin() + idx, m_components.end()};
    }

    void
    update_root();

    // clang-format off
    KRG_ar_function(
        category = "world",
        mcp_hint = "Adds relative offset [x y z] to current position"
    );
    void
    move(const vec3& v);
    // clang-format on

    const std::vector<component*>&
    get_subcomponents()
    {
        return m_components;
    }

protected:
    void
    recreate_structure_from_layout_impl(component* parent,
                                        uint32_t& position,
                                        uint32_t& total_subojects_count);

    void
    fill_renderable_components();

    // clang-format off
    KRG_ar_property(
        category                     = "Meta",
        serializable                 = true,
        property_save_handler        = game_object_components_save,
        property_compare_handler     = game_object_components_compare,
        property_copy_handler        = game_object_components_copy,
        property_instantiate_handler = game_object_components_instantiate,
        property_load_handler        = game_object_components__load
    );
    std::vector<component*> m_components;
    // clang-format on

    std::vector<game_object_component*> m_renderable_components;

    game_object_component* m_root_component = nullptr;
};

}  // namespace root
}  // namespace kryga
