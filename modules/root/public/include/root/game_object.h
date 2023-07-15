#pragma once

#include "root/game_object.generated.h"

#include "core/model_minimal.h"
#include "core/id_generator.h"

#include "root/core_types/vec3.h"
#include "root/components/game_object_component.h"
#include "root/smart_object.h"

namespace agea
{
namespace root
{
class component;

AGEA_ar_class("architype=game_object");
class game_object : public smart_object
{
    AGEA_gen_meta__game_object();

public:
    // Meta part
    AGEA_gen_class_meta(game_object, smart_object);
    AGEA_gen_construct_params
    {
        vec3 pos = {0.f};
    };
    AGEA_gen_meta_api;

    bool
    construct(construct_params& params);

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
        return spawn_component(parent, T::AR_TYPE_id(),
                               glob::id_generator::getr().generate(id, T::AR_TYPE_id()), params)
            ->as<T>();
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
    on_tick(float)
    {
    }

    component*
    get_component_at(size_t idx) const
    {
        AGEA_check(idx < m_components.size(), "Index should be in range");
        return m_components[idx];
    }

    AGEA_ar_function("category=world");
    glm::quat
    get_rotation_quat() const
    {
        return m_root_component->get_rotation();
    }

    AGEA_ar_function("category=world");
    vec3
    get_forward_vector() const
    {
        return m_root_component->get_forward_vector();
    }

    AGEA_ar_function("category=world");
    vec3
    get_up_vector() const
    {
        return m_root_component->get_up_vector();
    }

    AGEA_ar_function("category=world");
    vec3
    get_right_vector() const
    {
        return m_root_component->get_right_vector();
    }

    vec3
    get_position() const
    {
        return m_root_component->get_position();
    }

    void
    set_position(vec3 v)
    {
        m_root_component->set_position(v);
    }

    vec3
    get_rotation() const
    {
        return m_root_component->get_rotation();
    }

    void
    set_rotation(vec3 v);

    vec3
    get_scale() const
    {
        return m_root_component->get_scale();
    }

    void
    set_scale(vec3 v)
    {
        m_root_component->set_scale(v);
    }

    void
    set_root_component(game_object_component* root)
    {
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

    void
    move(const vec3& v);

protected:
    void
    recreate_structure_form_layout_impl(component* parent,
                                        uint32_t& position,
                                        uint32_t& total_subojects_count);

    void
    fill_renderable_components();

    AGEA_ar_property("category=meta",
                     "serializable=true",
                     "property_des_handler=custom::game_object_components_deserialize",
                     "property_ser_handler=custom::game_object_components_serialize",
                     "property_prototype_handler=custom::game_object_components_prototype",
                     "property_compare_handler=custom::game_object_components_compare",
                     "property_copy_handler=custom::game_object_components_copy");
    std::vector<component*> m_components;

    std::vector<game_object_component*> m_renderable_components;

    game_object_component* m_root_component = nullptr;
};

}  // namespace root
}  // namespace agea
