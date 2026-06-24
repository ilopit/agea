#include <gtest/gtest.h>

#include <asset_converter/converter_context.h>
#include <asset_converter/obj_parser.h>

#include <gpu_types/gpu_vertex_types.h>

#include <filesystem>

using namespace kryga::converter;

namespace
{

size_t
vertex_count(const std::vector<uint8_t>& bytes)
{
    return bytes.size() / sizeof(kryga::gpu::vertex_data);
}

const kryga::gpu::vertex_data*
vertex_data_ptr(const std::vector<uint8_t>& bytes)
{
    return reinterpret_cast<const kryga::gpu::vertex_data*>(bytes.data());
}

}  // namespace

class ObjParserTest : public ::testing::Test
{
protected:
    std::string m_cube_path;

    void
    SetUp() override
    {
        m_cube_path = "../packages/root.apkg/class/meshes/cube_mesh.obj";
        if (!std::filesystem::exists(m_cube_path))
        {
            GTEST_SKIP() << "cube_mesh.obj not found at " << m_cube_path;
        }
    }
};

TEST_F(ObjParserTest, ParseCubeObj)
{
    parsed_scene scene;
    ASSERT_TRUE(parse_obj(m_cube_path, scene));

    EXPECT_EQ(scene.name, "cube_mesh");
}

TEST_F(ObjParserTest, ExtractsMeshes)
{
    parsed_scene scene;
    ASSERT_TRUE(parse_obj(m_cube_path, scene));

    EXPECT_GT(scene.meshes.size(), 0u);

    for (const auto& mesh : scene.meshes)
    {
        EXPECT_GT(vertex_count(mesh.vertices), 0u);
        EXPECT_GT(mesh.indices.size(), 0u);
        EXPECT_EQ(mesh.indices.size() % 3, 0u);
    }
}

TEST_F(ObjParserTest, CreatesNodes)
{
    parsed_scene scene;
    ASSERT_TRUE(parse_obj(m_cube_path, scene));

    // OBJ parser creates one node per shape
    EXPECT_EQ(scene.nodes.size(), scene.meshes.size());
    EXPECT_EQ(scene.root_nodes.size(), scene.meshes.size());

    for (const auto& node : scene.nodes)
    {
        EXPECT_GE(node.mesh_index, 0);
        EXPECT_LT(static_cast<size_t>(node.mesh_index), scene.meshes.size());
    }
}

TEST_F(ObjParserTest, VertexDeduplication)
{
    parsed_scene scene;
    ASSERT_TRUE(parse_obj(m_cube_path, scene));
    ASSERT_GT(scene.meshes.size(), 0u);

    const auto& mesh = scene.meshes[0];
    // With deduplication, vertex count should be <= index count
    EXPECT_LE(vertex_count(mesh.vertices), mesh.indices.size());
}

TEST_F(ObjParserTest, NormalsAreComputed)
{
    parsed_scene scene;
    ASSERT_TRUE(parse_obj(m_cube_path, scene));
    ASSERT_GT(scene.meshes.size(), 0u);

    const auto& mesh = scene.meshes[0];
    size_t vert_count = vertex_count(mesh.vertices);
    const auto* verts = vertex_data_ptr(mesh.vertices);

    for (size_t i = 0; i < vert_count; ++i)
    {
        const auto& v = verts[i];
        // Normals should be unit length (or close to it)
        float len = glm::length(v.normal);
        if (len > 0.0f)
        {
            EXPECT_NEAR(len, 1.0f, 0.01f);
        }
    }
}

TEST(ObjParserErrorTest, NonexistentFileReturnsFalse)
{
    parsed_scene scene;
    EXPECT_FALSE(parse_obj("nonexistent_file.obj", scene));
}
