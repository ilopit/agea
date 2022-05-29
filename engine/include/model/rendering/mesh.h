#pragma once

#include "model/smart_object.h"

#include <vector>
#include <string>

namespace agea
{
namespace render
{
struct mesh_data;
}

namespace model
{
class mesh : public smart_object
{
public:
    AGEA_gen_class_meta(mesh, smart_object);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

    bool
    prepare_for_rendering();

    const std::string&
    get_vertices() const
    {
        return m_vertices;
    }

    const std::string&
    get_indices() const
    {
        return m_indices;
    }

    const std::string&
    get_external_type() const
    {
        return m_external_type;
    }

    const std::string&
    get_external_path()
    {
        return m_external_path;
    }

    render::mesh_data*
    get_mesh_data()
    {
        return m_mesh_data;
    }

protected:
    AGEA_property("category=assets", "serializable=true", "visible=true");
    std::string m_vertices;
    AGEA_property("category=assets", "serializable=true", "visible=true");
    std::string m_indices;
    AGEA_property("category=assets", "serializable=true", "visible=true");
    std::string m_external_type;
    AGEA_property("category=assets", "serializable=true", "visible=true");
    std::string m_external_path;

    uint32_t m_index_size = 0;

    render::mesh_data* m_mesh_data = nullptr;
};

}  // namespace model
}  // namespace agea
