#pragma once

#include "game_object.generated.h"

#include "model/model_minimal.h"
#include "model/core_types/vec3.h"
#include "model/components/game_object_component.h"

namespace agea
{
namespace model
{
class component;

AGEA_class();
class game_object : public smart_object
{
    AGEA_gen_meta__game_object();

public:
    // Meta part
    AGEA_gen_class_meta(game_object, smart_object);
    AGEA_gen_construct_params{

    };
    AGEA_gen_meta_architype_api(game_object);

    bool
    construct(construct_params& params)
    {
        return base_class::construct(params);
    }

    AGEA_function("category=world");
    glm::quat
    get_rotation_quat() const
    {
        return m_root_component->get_rotation();
    }

    AGEA_function("category=world");
    vec3
    get_forward_vector() const
    {
        return m_root_component->get_forward_vector();
    }

    AGEA_function("category=world");
    vec3
    get_up_vector() const
    {
        return m_root_component->get_up_vector();
    }

    AGEA_function("category=world");
    vec3
    get_right_vector() const
    {
        return m_root_component->get_right_vector();
    }
    virtual void
    on_tick(float dt)
    {
    }

    void
    attach_component(component* c)
    {
        c->set_owner(this);
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

    void
    prepare_for_rendering();

    void
    build_components_structure();

    bool
    post_construct();

    component*
    get_component_at(size_t idx) const
    {
        AGEA_check(idx < m_components.size(), "Index should be in range");
        return m_components[idx];
    }

    template <typename T>
    static void
    over_tickable(game_object_component* obj, const T& pred)
    {
        pred(obj);
        for (auto o : obj->get_render_components())
        {
            over_tickable<T>(o, pred);
        }
    }

protected:
    AGEA_property("category=world", "access=read_only", "hint=x,y,z", "ref=true");
    vec3* m_position = nullptr;

    AGEA_property("category=world", "access=read_only", "hint=x,y,z", "ref=true");
    vec3* m_rotation = nullptr;

    AGEA_property("category=world", "access=read_only", "hint=x,y,z", "ref=true");
    vec3* m_scale = nullptr;

    AGEA_property("category=meta",
                  "serializable=true",
                  "property_des_handler=custom::game_object_components_deserialize",
                  "property_ser_handler=custom::game_object_components_serialize",
                  "property_prototype_handler=custom::game_object_components_prototype",
                  "property_compare_handler=custom::game_object_components_compare",
                  "property_copy_handler=custom::game_object_components_copy");
    std::vector<component*> m_components;

    std::vector<component*> m_render_components;

    game_object_component* m_root_component = nullptr;

    bool m_updated = false;
};

}  // namespace model
}  // namespace agea
