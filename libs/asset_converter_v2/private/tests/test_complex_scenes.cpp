#include "engine_test_fixture.h"

#include <asset_converter/gltf_parser.h>

namespace fs = std::filesystem;
using namespace kryga::converter;

// ============================================================================
// LightsPunctualLamp - Khronos sample with KHR_lights_punctual
// 5 meshes, 6 materials, punctual lights
// ============================================================================

class LightsPunctualLampTest : public EngineTestFixture
{
protected:
    std::string m_gltf_path;
    parsed_scene m_scene;
    bool m_parsed = false;

    void
    SetUp() override
    {
        EngineTestFixture::SetUp();

        auto test_assets = find_test_assets();
        if (test_assets.empty())
        {
            return;
        }

        m_gltf_path = (test_assets / "LightsPunctualLamp.glb").string();
        if (!fs::exists(m_gltf_path))
        {
            return;
        }

        m_parsed = parse_gltf(m_gltf_path, m_scene);
    }
};

TEST_F(LightsPunctualLampTest, ParsesSuccessfully)
{
    if (m_gltf_path.empty())
    {
        GTEST_SKIP() << "LightsPunctualLamp.glb not found";
    }
    ASSERT_TRUE(m_parsed);
    EXPECT_EQ(m_scene.name, "LightsPunctualLamp");
}

TEST_F(LightsPunctualLampTest, ExtractsMultipleMeshes)
{
    if (!m_parsed)
    {
        GTEST_SKIP() << "LightsPunctualLamp.glb not parsed";
    }
    EXPECT_GE(m_scene.meshes.size(), 3u) << "Expected multiple meshes in lamp model";

    for (const auto& mesh : m_scene.meshes)
    {
        EXPECT_FALSE(mesh.name.empty());
        EXPECT_GT(mesh.vertices.size(), 0u);
        EXPECT_GT(mesh.indices.size(), 0u);
        EXPECT_EQ(mesh.indices.size() % 3, 0u);
    }
}

TEST_F(LightsPunctualLampTest, ExtractsMultipleMaterials)
{
    if (!m_parsed)
    {
        GTEST_SKIP() << "LightsPunctualLamp.glb not parsed";
    }
    EXPECT_GE(m_scene.materials.size(), 3u) << "Expected multiple materials";

    for (const auto& mat : m_scene.materials)
    {
        EXPECT_FALSE(mat.name.empty());
        EXPECT_FALSE(mat.shader_effect.empty());
    }
}

TEST_F(LightsPunctualLampTest, ExtractsLights)
{
    if (!m_parsed)
    {
        GTEST_SKIP() << "LightsPunctualLamp.glb not parsed";
    }
    EXPECT_GT(m_scene.lights.size(), 0u) << "Expected KHR_lights_punctual lights";

    for (const auto& light : m_scene.lights)
    {
        EXPECT_FALSE(light.name.empty());
        // Color components should be in [0,1] range typically
        EXPECT_GE(light.color.r, 0.f);
        EXPECT_GE(light.color.g, 0.f);
        EXPECT_GE(light.color.b, 0.f);
        EXPECT_GT(light.intensity, 0.f);
    }
}

TEST_F(LightsPunctualLampTest, LightsLinkedToNodes)
{
    if (!m_parsed)
    {
        GTEST_SKIP() << "LightsPunctualLamp.glb not parsed";
    }
    ASSERT_GT(m_scene.lights.size(), 0u);

    int light_nodes = 0;
    for (const auto& node : m_scene.nodes)
    {
        if (node.light_index >= 0)
        {
            ++light_nodes;
            EXPECT_LT(static_cast<size_t>(node.light_index), m_scene.lights.size());
        }
    }
    EXPECT_GT(light_nodes, 0) << "Lights exist but no nodes reference them";
}

TEST_F(LightsPunctualLampTest, ExtractsTextures)
{
    if (!m_parsed)
    {
        GTEST_SKIP() << "LightsPunctualLamp.glb not parsed";
    }
    EXPECT_GT(m_scene.textures.size(), 0u);

    for (const auto& tex : m_scene.textures)
    {
        EXPECT_GT(tex.width, 0u);
        EXPECT_GT(tex.height, 0u);
    }
}

TEST_F(LightsPunctualLampTest, FullPipelineWritesPackage)
{
    if (!m_parsed || m_data_root.empty())
    {
        GTEST_SKIP() << "LightsPunctualLamp.glb not parsed or data root not found";
    }

    ASSERT_TRUE(convert_scene(m_scene, "lamp"));

    fs::path pkg = package_dir("lamp");

    // Verify structure
    EXPECT_TRUE(fs::exists(pkg / "package.acfg"));
    EXPECT_TRUE(fs::exists(pkg / "class" / "meshes"));
    EXPECT_TRUE(fs::exists(pkg / "class" / "materials"));
    EXPECT_TRUE(fs::exists(pkg / "binaries"));

    // Count binary files
    int bin_count = count_files_with_extension(pkg / "binaries", ".abin");
    EXPECT_GE(bin_count, 3) << "Expected multiple binary files";
}

TEST_F(LightsPunctualLampTest, FullPipelineWritesLevel)
{
    if (!m_parsed || m_data_root.empty())
    {
        GTEST_SKIP() << "LightsPunctualLamp.glb not parsed or data root not found";
    }

    ASSERT_TRUE(convert_scene_with_level(m_scene, "lamp"));

    fs::path lvl = level_dir("lamp");

    EXPECT_TRUE(fs::exists(lvl / "root.cfg"));
    EXPECT_TRUE(fs::exists(lvl / "game_objects"));

    // Should have game objects for both meshes and lights
    std::string cfg = read_file(lvl / "root.cfg");
    EXPECT_NE(cfg.find("lamp.apkg"), std::string::npos);

    // Count game object files
    int go_count = count_files_with_extension(lvl / "game_objects", ".aobj");
    EXPECT_GT(go_count, 0) << "Expected game objects for meshes and/or lights";

    // Check that at least one light game object was generated
    bool has_light_go = false;
    for (auto& entry : fs::directory_iterator(lvl / "game_objects"))
    {
        if (entry.path().extension() != ".aobj")
        {
            continue;
        }

        std::string content = read_file(entry.path());
        if (content.find("_light") != std::string::npos)
        {
            has_light_go = true;
            break;
        }
    }
    EXPECT_TRUE(has_light_go) << "Expected at least one light game object in level";
}

// ============================================================================
// CesiumMilkTruck - Multi-mesh, textured, animated
// ============================================================================

class CesiumMilkTruckTest : public EngineTestFixture
{
protected:
    std::string m_gltf_path;
    parsed_scene m_scene;
    bool m_parsed = false;

    void
    SetUp() override
    {
        EngineTestFixture::SetUp();

        auto test_assets = find_test_assets();
        if (test_assets.empty())
        {
            return;
        }

        m_gltf_path = (test_assets / "CesiumMilkTruck.glb").string();
        if (!fs::exists(m_gltf_path))
        {
            return;
        }

        m_parsed = parse_gltf(m_gltf_path, m_scene);
    }
};

TEST_F(CesiumMilkTruckTest, ParsesSuccessfully)
{
    if (m_gltf_path.empty())
    {
        GTEST_SKIP() << "CesiumMilkTruck.glb not found";
    }
    ASSERT_TRUE(m_parsed);
    EXPECT_EQ(m_scene.name, "CesiumMilkTruck");
}

TEST_F(CesiumMilkTruckTest, ExtractsMultipleMeshes)
{
    if (!m_parsed)
    {
        GTEST_SKIP() << "CesiumMilkTruck.glb not parsed";
    }
    EXPECT_GE(m_scene.meshes.size(), 2u) << "Truck should have multiple mesh parts";
}

TEST_F(CesiumMilkTruckTest, ExtractsMaterials)
{
    if (!m_parsed)
    {
        GTEST_SKIP() << "CesiumMilkTruck.glb not parsed";
    }
    EXPECT_GE(m_scene.materials.size(), 1u);
}

TEST_F(CesiumMilkTruckTest, ExtractsTextures)
{
    if (!m_parsed)
    {
        GTEST_SKIP() << "CesiumMilkTruck.glb not parsed";
    }
    EXPECT_GT(m_scene.textures.size(), 0u);
}

TEST_F(CesiumMilkTruckTest, HasSceneGraph)
{
    if (!m_parsed)
    {
        GTEST_SKIP() << "CesiumMilkTruck.glb not parsed";
    }
    EXPECT_GT(m_scene.nodes.size(), 0u);
    EXPECT_GT(m_scene.root_nodes.size(), 0u);

    // Verify some nodes reference meshes
    int mesh_node_count = 0;
    for (const auto& node : m_scene.nodes)
    {
        if (node.mesh_index >= 0)
        {
            ++mesh_node_count;
        }
    }
    EXPECT_GT(mesh_node_count, 0);
}

TEST_F(CesiumMilkTruckTest, FullPipelineEndToEnd)
{
    if (!m_parsed || m_data_root.empty())
    {
        GTEST_SKIP() << "CesiumMilkTruck.glb not parsed or data root not found";
    }

    // Write package and level together
    ASSERT_TRUE(convert_scene_with_level(m_scene, "truck"));

    EXPECT_TRUE(fs::exists(package_dir("truck") / "package.acfg"));
    EXPECT_TRUE(fs::exists(level_dir("truck") / "root.cfg"));
}
