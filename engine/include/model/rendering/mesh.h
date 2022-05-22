#pragma once

#include "model/object_constructor.h"
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

    AGEA_property("category=assets", "serializable=true", "visible=true");
    std::string m_vertices;
    AGEA_property("category=assets", "serializable=true", "visible=true");
    std::string m_indices;
    AGEA_property("category=assets", "serializable=true", "visible=true");
    std::string m_external_type;
    AGEA_property("category=assets", "serializable=true", "visible=true");
    std::string m_external_path;

    uint32_t m_index_size = 0;

    render::mesh_data* m_mesh_data;
};

}  // namespace model
}  // namespace agea
