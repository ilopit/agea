#pragma once

#include "packages/root/model/mesh.ar.h"

#include "packages/root/model/assets/asset.h"

#include "utils/buffer.h"

#include <vector>
#include <string>

namespace agea
{
namespace render
{
class mesh_data;
}

namespace root
{
// clang-format off
AGEA_ar_class("architype=mesh",
              render_constructor = mesh__render_loader,
              render_destructor  = mesh__render_destructor);
class mesh : public asset
// clang-format on
{
    AGEA_gen_meta__mesh();

public:
    AGEA_gen_class_meta(mesh, asset);
    AGEA_gen_construct_params
    {
        utils::buffer vertices;
        utils::buffer indices;
        utils::buffer external;
    };
    AGEA_gen_meta_api;

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
    get_indices_buffer()
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

    bool
    construct(this_class::construct_params& params);

protected:
    AGEA_ar_property("category=assets", "serializable=true", "default=true");
    utils::buffer m_vertices = {};

    AGEA_ar_property("category=assets", "serializable=true", "default=true");
    utils::buffer m_indices = {};

    AGEA_ar_property("category=assets", "serializable=true", "default=true");
    utils::buffer m_external = {};

    render::mesh_data* m_mesh_data = nullptr;
};

}  // namespace root
}  // namespace agea
