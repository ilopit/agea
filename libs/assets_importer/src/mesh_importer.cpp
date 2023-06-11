#include "assets_importer/assets_importer.h"

#include <utils/agea_log.h>
#include <utils/buffer.h>

#include <vulkan_render/types/vulkan_gpu_types.h>

#include <tiny_obj_loader.h>

#include <iostream>

namespace agea
{
namespace asset_importer
{
namespace mesh_importer
{
bool
extract_mesh_data_from_3do(const utils::path& obj_path,
                           utils::buffer_view<render::gpu_vertex_data> verices,
                           utils::buffer_view<render::gpu_index_data> indices)
{
    // attrib will contain the vertex arrays of the file
    tinyobj::attrib_t attrib;
    // shapes contains the info for each separate object in the file
    std::vector<tinyobj::shape_t> shapes;
    // materials contains the information about the material of each shape, but we wont use it.
    std::vector<tinyobj::material_t> materials;

    // error and warning output from the load function
    std::string warn;
    std::string err;

    // load the OBJ file
    tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, obj_path.str().c_str(), nullptr);
    // make sure to output the warnings to the console, in case there are issues with the file
    if (!warn.empty())
    {
        ALOG_WARN("{0}", warn.c_str());
    }
    // if we have any error, print it to the console, and break the mesh loading.
    // This happens if the file cant be found or is malformed
    if (!err.empty())
    {
        ALOG_WARN("{0}", err.c_str());
        return false;
    }

    // Loop over shapes
    for (size_t s = 0; s < shapes.size(); s++)
    {
        // Loop over faces(polygon)
        verices.resize(3 * shapes[s].mesh.num_face_vertices.size());

        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
        {
            // hardcode loading to triangles
            uint32_t fv = 3;

            // Loop over vertices in the face.
            for (uint32_t v = 0; v < fv; v++)
            {
                // access to vertex
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                // vertex position
                tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
                tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
                tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];

                // vertex normal
                tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
                tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
                tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];

                // vertex uv
                tinyobj::real_t ux = attrib.texcoords[2 * idx.texcoord_index + 0];
                tinyobj::real_t uy = attrib.texcoords[2 * idx.texcoord_index + 1];

                tinyobj::real_t r = attrib.colors[3 * idx.vertex_index + 0];
                tinyobj::real_t g = attrib.colors[3 * idx.vertex_index + 1];
                tinyobj::real_t b = attrib.colors[3 * idx.vertex_index + 2];

                // copy it into our vertex
                render::gpu_vertex_data new_vert;
                new_vert.position.x = vx;
                new_vert.position.y = vy;
                new_vert.position.z = vz;

                new_vert.normal.x = nx;
                new_vert.normal.y = ny;
                new_vert.normal.z = nz;

                new_vert.uv.x = ux;
                new_vert.uv.y = 1 - uy;

                new_vert.color.r = r;
                new_vert.color.g = g;
                new_vert.color.b = b;

                verices.at((uint32_t)(index_offset + v)) = new_vert;
            }
            index_offset += fv;
        }
    }

    indices.resize(verices.size());

    for (uint32_t i = 0; i < verices.size(); i++)
    {
        indices.at(i) = render::gpu_index_data(i);
    }
    return true;
}

}  // namespace mesh_importer
}  // namespace asset_importer
}  // namespace agea