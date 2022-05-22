#include "vulkan_render/data_loaders/vulkan_mesh_data_loader.h"

#include "vulkan_render/vulkan_mesh_data.h"
#include "utils/file_utils.h"

#include <tiny_obj_loader.h>
#include <iostream>

namespace agea
{
namespace render
{

namespace
{
struct vertex_f32_pncv
{
    float position[3];
    float normal[3];
    float color[3];
    float uv[2];
};
}  // namespace

bool
vulkan_mesh_data_loader::load_from_obj(const std::string& filename, mesh_data& md)
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
    tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str(), nullptr);
    // make sure to output the warnings to the console, in case there are issues with the file
    if (!warn.empty())
    {
        std::cout << "WARN: " << warn << std::endl;
    }
    // if we have any error, print it to the console, and break the mesh loading.
    // This happens if the file cant be found or is malformed
    if (!err.empty())
    {
        std::cerr << err << std::endl;
        return false;
    }

    // Loop over shapes
    for (size_t s = 0; s < shapes.size(); s++)
    {
        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
        {
            // hardcode loading to triangles
            int fv = 3;

            // Loop over vertices in the face.
            for (size_t v = 0; v < fv; v++)
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
                vertex_data new_vert;
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

                md.m_vertices.push_back(new_vert);
            }
            index_offset += fv;
        }
    }

    for (int i = 0; i < md.m_vertices.size(); i++)
    {
        md.m_indices.push_back(i);
    }
    return true;
}

bool
vulkan_mesh_data_loader::load_from_amsh(const std::string& index_file,
                                        const std::string& verteces_file,
                                        mesh_data& md)
{
    std::vector<char> indexBuffer, vertexBuffer;

    if (!file_utils::load_file(index_file, indexBuffer))
    {
        return false;
    }

    if (!file_utils::load_file(verteces_file, vertexBuffer))
    {
        return false;
    }

    md.m_indices.resize(indexBuffer.size() / sizeof(uint32_t));
    for (int i = 0; i < md.m_indices.size(); i++)
    {
        uint32_t* unpacked_indices = (uint32_t*)indexBuffer.data();
        md.m_indices[i] = unpacked_indices[i];
    }

    vertex_f32_pncv* unpackedVertices = (vertex_f32_pncv*)vertexBuffer.data();

    md.m_vertices.resize(vertexBuffer.size() / sizeof(vertex_f32_pncv));

    for (int i = 0; i < md.m_vertices.size(); i++)
    {
        md.m_vertices[i].position.x = unpackedVertices[i].position[0];
        md.m_vertices[i].position.y = unpackedVertices[i].position[1];
        md.m_vertices[i].position.z = unpackedVertices[i].position[2];
        md.m_vertices[i].normal.x = unpackedVertices[i].normal[0];
        md.m_vertices[i].normal.y = unpackedVertices[i].normal[1];
        md.m_vertices[i].normal.z = unpackedVertices[i].normal[2];
        md.m_vertices[i].color.x = unpackedVertices[i].color[0];
        md.m_vertices[i].color.y = unpackedVertices[i].color[1];
        md.m_vertices[i].color.z = unpackedVertices[i].color[2];
        md.m_vertices[i].uv.x = unpackedVertices[i].uv[0];
        md.m_vertices[i].uv.y = unpackedVertices[i].uv[1];
    }
    return true;
}

}  // namespace render
}  // namespace agea
