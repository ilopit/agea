﻿#pragma once

#include "mesh.generated.h"

#include "model/assets/asset.h"

#include <vector>
#include <string>

namespace agea
{
namespace render
{
class mesh_data;
}

namespace model
{
AGEA_class();
class mesh : public smart_object
{
    AGEA_gen_meta__mesh();

public:
    AGEA_gen_class_meta(mesh, smart_object);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_architype_api(mesh);

    render::mesh_data*
    get_mesh_data()
    {
        return m_mesh_data;
    }

    void
    set_mesh_data(render::mesh_data* v)
    {
        m_mesh_data = v;
    }

    utils::buffer&
    get_vertices_buffer()
    {
        return m_vertices;
    }

    void
    set_vertices_buffer(utils::buffer& v)
    {
        m_vertices = v;
    }

    utils::buffer&
    get_indicess_buffer()
    {
        return m_indices;
    }

    void
    set_indices_buffer(utils::buffer& v)
    {
        m_indices = v;
    }

    utils::buffer&
    get_external_buffer()
    {
        return m_external;
    }

    void
    set_external_buffer(utils::buffer& v)
    {
        m_external = v;
    }

protected:
    AGEA_property("category=assets", "serializable=true", "default=true");
    utils::buffer m_vertices = {};

    AGEA_property("category=assets", "serializable=true", "default=true");
    utils::buffer m_indices = {};

    AGEA_property("category=assets", "serializable=true", "default=true");
    utils::buffer m_external = {};

    render::mesh_data* m_mesh_data = nullptr;
};

}  // namespace model
}  // namespace agea
