#pragma once

#include "packages/root/model/mesh.ar.h"

#include "packages/root/model/assets/asset.h"
#include "packages/root/model/core_types/vec3.h"

#include "utils/buffer.h"

#include <vector>
#include <string>

namespace kryga
{
namespace root
{
// clang-format off
KRG_ar_class(
    "architype=mesh",
    render_cmd_builder   = mesh__cmd_builder,
    render_cmd_destroyer = mesh__cmd_destroyer,
    mcp_schema           = "string:asset_id",
    mcp_hint             = "3D geometry data — vertex positions / normals / UVs and triangle "
                           "indices"
);
class mesh : public asset
// clang-format on
{
    KRG_gen_meta__mesh();

public:
    KRG_gen_class_meta(mesh, asset);
    KRG_gen_construct_params
    {
        utils::buffer vertices;
        utils::buffer indices;
        utils::buffer external;
    };
    KRG_gen_meta_api;

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

    float
    get_bounding_radius() const
    {
        return m_bounding_radius;
    }
    void
    set_bounding_radius(float r)
    {
        m_bounding_radius = r;
    }

    const ::kryga::root::vec3&
    get_local_centroid() const
    {
        return m_local_centroid;
    }
    void
    set_local_centroid(const ::kryga::root::vec3& c)
    {
        m_local_centroid = c;
    }

protected:
    // clang-format off
    KRG_ar_property(
        "category=assets",
        "serializable=true",
        "default=true",
        "mcp_hint=vertex buffer binary — read-only at runtime"
    );
    utils::buffer m_vertices = {};
    // clang-format on

    // clang-format off
    KRG_ar_property(
        "category=assets",
        "serializable=true",
        "default=true",
        "mcp_hint=index buffer binary — read-only at runtime"
    );
    utils::buffer m_indices = {};
    // clang-format on

    // clang-format off
    KRG_ar_property(
        "category=assets",
        "serializable=true",
        "default=true",
        "mcp_hint=additional mesh data like UV2 — read-only at runtime"
    );
    utils::buffer m_external = {};
    // clang-format on

    float m_bounding_radius = 0.0f;
    ::kryga::root::vec3 m_local_centroid = {};
};

}  // namespace root
}  // namespace kryga
