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

class LightCameraWriterTest : public EngineTestFixture
{
protected:
    parsed_scene m_scene;

    // Add a minimal mesh to the scene so the package can be created
    void
    add_dummy_mesh()
    {
        parsed_mesh mesh;
        mesh.name = "dummy";
        mesh.vertices = make_vertex_bytes({
            {glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), glm::vec3(1, 1, 1), glm::vec2(0, 0)},
        });
        mesh.indices = {0};
        m_scene.meshes.push_back(std::move(mesh));

        parsed_material mat;
        mat.name = "dummy_mat";
        m_scene.materials.push_back(std::move(mat));
    }
};

TEST_F(LightCameraWriterTest, WritesPointLight)
{
    if (m_data_root.empty())
    {
        GTEST_SKIP() << "Data root not found";
    }

    add_dummy_mesh();  // Package needs at least one asset

    parsed_light light;
    light.name = "my_light";
    light.type = parsed_light_type::point;
    light.color = glm::vec3(1.f, 0.5f, 0.2f);
    light.intensity = 1.0f;
    light.range = 25.f;
    m_scene.lights.push_back(std::move(light));

    parsed_node node;
    node.name = "light_node";
    node.light_index = 0;
    node.transform.position = glm::vec3(5.f, 10.f, 3.f);
    m_scene.nodes.push_back(std::move(node));
    m_scene.root_nodes.push_back(0);

    ASSERT_TRUE(convert_scene_with_level(m_scene, "test_light"));

    fs::path go = level_dir("test_light") / "game_objects" / "light_node.aobj";
    ASSERT_TRUE(fs::exists(go));

    std::string content = read_file(go);
    EXPECT_NE(content.find("proto_id: point_light"), std::string::npos);
    // Engine serialization format - light properties are serialized by reflection
    EXPECT_FALSE(content.empty()) << "Light game object file should not be empty";
}

TEST_F(LightCameraWriterTest, WritesDirectionalLight)
{
    if (m_data_root.empty())
    {
        GTEST_SKIP() << "Data root not found";
    }

    add_dummy_mesh();  // Package needs at least one asset

    parsed_light light;
    light.name = "sun";
    light.type = parsed_light_type::directional;
    light.color = glm::vec3(1.f, 1.f, 0.9f);
    light.intensity = 0.8f;
    m_scene.lights.push_back(std::move(light));

    parsed_node node;
    node.name = "sun_node";
    node.light_index = 0;
    m_scene.nodes.push_back(std::move(node));
    m_scene.root_nodes.push_back(0);

    ASSERT_TRUE(convert_scene_with_level(m_scene, "test_sun"));

    fs::path go = level_dir("test_sun") / "game_objects" / "sun_node.aobj";
    ASSERT_TRUE(fs::exists(go));

    std::string content = read_file(go);
    EXPECT_NE(content.find("proto_id: directional_light"), std::string::npos);
    EXPECT_FALSE(content.empty()) << "Light game object file should not be empty";
}

TEST_F(LightCameraWriterTest, WritesSpotLight)
{
    if (m_data_root.empty())
    {
        GTEST_SKIP() << "Data root not found";
    }

    add_dummy_mesh();  // Package needs at least one asset

    parsed_light light;
    light.name = "spot";
    light.type = parsed_light_type::spot;
    light.color = glm::vec3(1.f);
    light.intensity = 1.0f;
    light.range = 30.f;
    light.inner_cone = 0.2f;
    light.outer_cone = 0.4f;
    m_scene.lights.push_back(std::move(light));

    parsed_node node;
    node.name = "spot_node";
    node.light_index = 0;
    m_scene.nodes.push_back(std::move(node));
    m_scene.root_nodes.push_back(0);

    ASSERT_TRUE(convert_scene_with_level(m_scene, "test_spot"));

    fs::path go = level_dir("test_spot") / "game_objects" / "spot_node.aobj";
    ASSERT_TRUE(fs::exists(go));

    std::string content = read_file(go);
    EXPECT_NE(content.find("proto_id: spot_light"), std::string::npos);
    EXPECT_FALSE(content.empty()) << "Light game object file should not be empty";
}

TEST_F(LightCameraWriterTest, WritesCamera)
{
    if (m_data_root.empty())
    {
        GTEST_SKIP() << "Data root not found";
    }

    add_dummy_mesh();  // Package needs at least one asset

    parsed_camera cam;
    cam.name = "main_cam";
    cam.fov = 75.f;
    cam.znear = 0.5f;
    cam.zfar = 500.f;
    m_scene.cameras.push_back(std::move(cam));

    parsed_node node;
    node.name = "camera_node";
    node.camera_index = 0;
    node.transform.position = glm::vec3(0.f, 5.f, -10.f);
    m_scene.nodes.push_back(std::move(node));
    m_scene.root_nodes.push_back(0);

    ASSERT_TRUE(convert_scene_with_level(m_scene, "test_cam"));

    fs::path go = level_dir("test_cam") / "game_objects" / "camera_node.aobj";
    ASSERT_TRUE(fs::exists(go));

    std::string content = read_file(go);
    EXPECT_NE(content.find("proto_id: camera_object"), std::string::npos);
    EXPECT_FALSE(content.empty()) << "Camera game object file should not be empty";
}

TEST_F(LightCameraWriterTest, MixedSceneWithMeshesAndLights)
{
    if (m_data_root.empty())
    {
        GTEST_SKIP() << "Data root not found";
    }

    // Mesh
    parsed_mesh mesh;
    mesh.name = "cube";
    mesh.vertices = make_vertex_bytes({
        {glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), glm::vec3(1, 1, 1), glm::vec2(0, 0)},
    });
    mesh.indices = {0};
    m_scene.meshes.push_back(std::move(mesh));

    parsed_material mat;
    mat.name = "cube_mat";
    m_scene.materials.push_back(std::move(mat));

    // Light
    parsed_light light;
    light.name = "lamp";
    light.type = parsed_light_type::point;
    m_scene.lights.push_back(std::move(light));

    // Nodes
    parsed_node mesh_node;
    mesh_node.name = "cube_node";
    mesh_node.mesh_index = 0;
    mesh_node.material_index = 0;
    m_scene.nodes.push_back(std::move(mesh_node));

    parsed_node light_node;
    light_node.name = "lamp_node";
    light_node.light_index = 0;
    m_scene.nodes.push_back(std::move(light_node));

    m_scene.root_nodes = {0, 1};

    ASSERT_TRUE(convert_scene_with_level(m_scene, "test_mixed"));

    fs::path lvl = level_dir("test_mixed");

    // Both should exist
    EXPECT_TRUE(fs::exists(lvl / "game_objects" / "cube_node.aobj"));
    EXPECT_TRUE(fs::exists(lvl / "game_objects" / "lamp_node.aobj"));

    // Manifest should reference both
    std::string cfg = read_file(lvl / "root.cfg");
    EXPECT_NE(cfg.find("cube_node"), std::string::npos);
    EXPECT_NE(cfg.find("lamp_node"), std::string::npos);
}
