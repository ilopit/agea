#pragma once

#include "demo_mesh_component.generated.h"

#include "model/components/mesh_component.h"

namespace agea
{
namespace model
{
AGEA_class();
class demo_mesh_component : public mesh_component
{
    AGEA_gen_meta__demo_mesh_component();

    virtual void
    on_tick(float dt) override
    {
    }

public:
    AGEA_gen_class_meta(demo_mesh_component, mesh_component);

    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

    AGEA_property("category=demo", "serializable=true", "access=no");
    std::string m_kind;

    float time = 0.f;
    float fleep_time = 5.f;

    bool fleep = true;
    float directon = 1.f;
};
}  // namespace model
}  // namespace agea
