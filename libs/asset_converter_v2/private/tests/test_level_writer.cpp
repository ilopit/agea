#include "engine_test_fixture.h"

#include <gpu_types/gpu_vertex_types.h>

#include <cstring>

namespace fs = std::filesystem;
using namespace kryga::converter;

namespace
{

std::vector<uint8_t>
make_vertex_bytes(const std::vector<kryga::gpu::vertex_data>& verts)
{
    std::vector<uint8_t> bytes(verts.size() * sizeof(kryga::gpu::vertex_data));
    std::memcpy(bytes.data(), verts.data(), bytes.size());
    return bytes;
}

}  // namespace

class LevelWriterTest : public EngineTestFixture
{
protected:
    parsed_scene m_scene;

    void
    SetUp() override
    {
        EngineTestFixture::SetUp();

        // Build a minimal scene with renderable nodes
        parsed_mesh mesh;
        mesh.name = "cube";
        mesh.vertices = make_vertex_bytes({
            {.position = glm::vec3(0, 0, 0),
             .normal = glm::vec3(0, 1, 0),
             .color = glm::vec3(1, 1, 1),
             .uv = glm::vec2(0, 0)},
            {.position = glm::vec3(1, 0, 0),
             .normal = glm::vec3(0, 1, 0),
             .color = glm::vec3(1, 1, 1),
             .uv = glm::vec2(1, 0)},
            {.position = glm::vec3(0, 1, 0),
             .normal = glm::vec3(0, 1, 0),
             .color = glm::vec3(1, 1, 1),
             .uv = glm::vec2(0, 1)},
        });
        mesh.indices = {0, 1, 2};
        m_scene.meshes.push_back(std::move(mesh));

        parsed_material mat;
        mat.name = "cube_mat";
        m_scene.materials.push_back(std::move(mat));

        parsed_node node;
        node.name = "cube_node";
        node.mesh_index = 0;
        node.material_index = 0;
        node.transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
        node.transform.rotation = glm::vec3(45.0f, 0.0f, 0.0f);
        node.transform.scale = glm::vec3(2.0f, 2.0f, 2.0f);
        m_scene.nodes.push_back(std::move(node));
        m_scene.root_nodes.push_back(0);
    }
};

TEST_F(LevelWriterTest, CreatesLevelStructure)
{
    if (m_data_root.empty())
    {
        GTEST_SKIP() << "Data root not found";
    }

    ASSERT_TRUE(convert_scene_with_level(m_scene, "test_lvl"));

    fs::path lvl = level_dir("test_lvl");
    EXPECT_TRUE(fs::exists(lvl / "root.cfg"));
    EXPECT_TRUE(fs::exists(lvl / "game_objects"));
}

TEST_F(LevelWriterTest, WritesRootCfg)
{
    if (m_data_root.empty())
    {
        GTEST_SKIP() << "Data root not found";
    }

    ASSERT_TRUE(convert_scene_with_level(m_scene, "test_lvl"));

    std::string cfg = read_file(level_dir("test_lvl") / "root.cfg");
    EXPECT_NE(cfg.find("packages:"), std::string::npos);
    EXPECT_NE(cfg.find("test_lvl.apkg"), std::string::npos);
    EXPECT_NE(cfg.find("instance_obj_mapping:"), std::string::npos);
    EXPECT_NE(cfg.find("cube_node"), std::string::npos);
}

TEST_F(LevelWriterTest, WritesGameObject)
{
    if (m_data_root.empty())
    {
        GTEST_SKIP() << "Data root not found";
    }

    ASSERT_TRUE(convert_scene_with_level(m_scene, "test_lvl"));

    fs::path go_path = level_dir("test_lvl") / "game_objects" / "cube_node.aobj";
    EXPECT_TRUE(fs::exists(go_path));

    std::string content = read_file(go_path);
    EXPECT_NE(content.find("proto_id: mesh_object"), std::string::npos);
    EXPECT_NE(content.find("id: cube_node"), std::string::npos);
    // Mesh reference is serialized within the component
    EXPECT_NE(content.find("cube"), std::string::npos) << "Should reference cube mesh";
}

TEST_F(LevelWriterTest, WritesTransform)
{
    if (m_data_root.empty())
    {
        GTEST_SKIP() << "Data root not found";
    }

    ASSERT_TRUE(convert_scene_with_level(m_scene, "test_lvl"));

    std::string content = read_file(level_dir("test_lvl") / "game_objects" / "cube_node.aobj");
    EXPECT_NE(content.find("position:"), std::string::npos);
    EXPECT_NE(content.find("rotation:"), std::string::npos);
    EXPECT_NE(content.find("scale:"), std::string::npos);
}

TEST_F(LevelWriterTest, SkipsNonRenderableNodes)
{
    if (m_data_root.empty())
    {
        GTEST_SKIP() << "Data root not found";
    }

    // Add a node with no mesh
    parsed_node empty_node;
    empty_node.name = "empty_parent";
    empty_node.children.push_back(0);  // child is the cube node
    m_scene.nodes.push_back(std::move(empty_node));

    // Replace root with the empty parent
    m_scene.root_nodes = {1};

    ASSERT_TRUE(convert_scene_with_level(m_scene, "test_lvl"));

    fs::path lvl = level_dir("test_lvl");
    // empty_parent is the root → game_object; cube_node is a child → component inside it
    EXPECT_TRUE(fs::exists(lvl / "game_objects" / "empty_parent.aobj"));
    EXPECT_FALSE(fs::exists(lvl / "game_objects" / "cube_node.aobj"));
}
