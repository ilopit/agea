#pragma once

#include "mesh.generated.h"

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
    AGEA_gen_meta__mesh();

public:
    AGEA_gen_class_meta(mesh, smart_object);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_architype_api(mesh);

    bool
    prepare_for_rendering();

    render::mesh_data*
    get_mesh_data()
    {
        return m_mesh_data;
    }

protected:
    AGEA_property("category=assets", "serializable=true");
    std::string m_vertices;

    AGEA_property("category=assets", "serializable=true");
    std::string m_indices;

    AGEA_property("category=assets", "serializable=true");
    std::string m_external_type;

    AGEA_property("category=assets", "serializable=true");
    std::string m_external_path;

    uint32_t m_index_size = 0;

    render::mesh_data* m_mesh_data = nullptr;
};

}  // namespace model
}  // namespace agea
