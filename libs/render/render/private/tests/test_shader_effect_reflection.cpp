#include <gtest/gtest.h>

#include <vulkan_render/shader_reflection_utils.h>
#include <vulkan_render/types/vulkan_shader_effect_data.h>
#include <vulkan_render/types/vulkan_gpu_types.h>

using namespace kryga;
using namespace kryga::render;

class shader_reflection_test : public ::testing::Test
{
protected:
    // Helper to create a UBO binding
    reflection::binding
    make_ubo_binding(const char* name, uint32_t location)
    {
        reflection::binding b;
        b.name = AID(name);
        b.location = location;
        b.descriptors_count = 1;
        b.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        return b;
    }

    // Helper to create an SSBO binding
    reflection::binding
    make_ssbo_binding(const char* name, uint32_t location)
    {
        reflection::binding b;
        b.name = AID(name);
        b.location = location;
        b.descriptors_count = 1;
        b.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        return b;
    }

    // Helper to create a descriptor set
    reflection::descriptor_set
    make_descriptor_set(uint32_t set_location, std::vector<reflection::binding> bindings)
    {
        reflection::descriptor_set ds;
        ds.location = set_location;
        ds.bindigns = std::move(bindings);
        return ds;
    }

    // Helper to create push constants
    reflection::push_constants
    make_push_constants(const char* name, uint32_t offset, uint32_t size)
    {
        reflection::push_constants pc;
        pc.name = name;
        pc.offset = offset;
        pc.size = size;
        return pc;
    }
};

// Test: convert_to_ds_layout_data with single UBO
TEST_F(shader_reflection_test, convert_single_ubo_to_layout)
{
    auto ds = make_descriptor_set(0, {make_ubo_binding("material", 0)});

    vulkan_descriptor_set_layout_data layout;
    vulkan_shader_reflection_utils::convert_to_ds_layout_data(ds, VK_SHADER_STAGE_FRAGMENT_BIT, layout);

    EXPECT_EQ(layout.set_idx, 0u);
    ASSERT_EQ(layout.bindings.size(), 1u);
    EXPECT_EQ(layout.bindings[0].binding, 0u);
    EXPECT_EQ(layout.bindings[0].descriptorType, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    EXPECT_EQ(layout.bindings[0].stageFlags, VK_SHADER_STAGE_FRAGMENT_BIT);
    EXPECT_EQ(layout.bindings[0].descriptorCount, 1u);
}

// Test: convert_to_ds_layout_data with single SSBO
TEST_F(shader_reflection_test, convert_single_ssbo_to_layout)
{
    auto ds = make_descriptor_set(0, {make_ssbo_binding("objects", 0)});

    vulkan_descriptor_set_layout_data layout;
    vulkan_shader_reflection_utils::convert_to_ds_layout_data(ds, VK_SHADER_STAGE_VERTEX_BIT, layout);

    EXPECT_EQ(layout.set_idx, 0u);
    ASSERT_EQ(layout.bindings.size(), 1u);
    EXPECT_EQ(layout.bindings[0].descriptorType, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    EXPECT_EQ(layout.bindings[0].stageFlags, VK_SHADER_STAGE_VERTEX_BIT);
}

// Test: convert_to_ds_layout_data with multiple bindings (UBO + SSBO)
TEST_F(shader_reflection_test, convert_multiple_bindings_to_layout)
{
    auto ds =
        make_descriptor_set(0, {make_ubo_binding("camera", 0), make_ssbo_binding("instances", 1)});

    vulkan_descriptor_set_layout_data layout;
    vulkan_shader_reflection_utils::convert_to_ds_layout_data(ds, VK_SHADER_STAGE_VERTEX_BIT, layout);

    EXPECT_EQ(layout.set_idx, 0u);
    ASSERT_EQ(layout.bindings.size(), 2u);

    // Binding 0: UBO
    EXPECT_EQ(layout.bindings[0].binding, 0u);
    EXPECT_EQ(layout.bindings[0].descriptorType, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    // Binding 1: SSBO
    EXPECT_EQ(layout.bindings[1].binding, 1u);
    EXPECT_EQ(layout.bindings[1].descriptorType, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
}

// Test: convert_to_ds_layout_data preserves set index
TEST_F(shader_reflection_test, convert_preserves_set_index)
{
    auto ds = make_descriptor_set(2, {make_ubo_binding("data", 0)});

    vulkan_descriptor_set_layout_data layout;
    vulkan_shader_reflection_utils::convert_to_ds_layout_data(ds, VK_SHADER_STAGE_FRAGMENT_BIT, layout);

    EXPECT_EQ(layout.set_idx, 2u);
}

// Test: convert_to_vk_push_constants
TEST_F(shader_reflection_test, convert_push_constants)
{
    auto pc = make_push_constants("PushConstants", 0, 80);  // mat4 + vec4 = 64 + 16

    VkPushConstantRange range;
    shader_reflection_utils::convert_to_vk_push_constants(pc, VK_SHADER_STAGE_VERTEX_BIT, range);

    EXPECT_EQ(range.offset, 0u);
    EXPECT_EQ(range.size, 80u);
    EXPECT_EQ(range.stageFlags, VK_SHADER_STAGE_VERTEX_BIT);
}

// Test: convert_to_vk_push_constants with offset
TEST_F(shader_reflection_test, convert_push_constants_with_offset)
{
    auto pc = make_push_constants("FragPush", 64, 16);  // Offset after vertex push constants

    VkPushConstantRange range;
    shader_reflection_utils::convert_to_vk_push_constants(pc, VK_SHADER_STAGE_FRAGMENT_BIT, range);

    EXPECT_EQ(range.offset, 64u);
    EXPECT_EQ(range.size, 16u);
    EXPECT_EQ(range.stageFlags, VK_SHADER_STAGE_FRAGMENT_BIT);
}

// Test: generate_set_layouts from shader_effect_data
TEST_F(shader_reflection_test, generate_set_layouts_from_reflection)
{
    shader_effect_data sed(AID("test_effect"));

    // Set up vertex stage reflection with set=0, binding=0 UBO
    sed.m_vertext_stage_reflection.descriptors.push_back(
        make_descriptor_set(0, {make_ubo_binding("vert_ubo", 0)}));

    // Set up fragment stage reflection with set=0, binding=1 UBO and set=1, binding=0 SSBO
    sed.m_fragment_stage_reflection.descriptors.push_back(
        make_descriptor_set(0, {make_ubo_binding("frag_ubo", 1)}));
    sed.m_fragment_stage_reflection.descriptors.push_back(
        make_descriptor_set(1, {make_ssbo_binding("frag_ssbo", 0)}));

    std::vector<vulkan_descriptor_set_layout_data> layouts;
    sed.generate_set_layouts(layouts);

    // Should have 3 entries (2 from fragment, 1 from vertex)
    ASSERT_EQ(layouts.size(), 3u);

    // Count bindings per set
    uint32_t set0_bindings = 0;
    uint32_t set1_bindings = 0;

    for (const auto& layout : layouts)
    {
        if (layout.set_idx == 0)
            set0_bindings += (uint32_t)layout.bindings.size();
        else if (layout.set_idx == 1)
            set1_bindings += (uint32_t)layout.bindings.size();
    }

    EXPECT_EQ(set0_bindings, 2u);  // One from vertex, one from fragment
    EXPECT_EQ(set1_bindings, 1u);  // One from fragment
}

// Test: generate_constants from shader_effect_data
TEST_F(shader_reflection_test, generate_constants_from_reflection)
{
    shader_effect_data sed(AID("test_effect"));

    // Vertex stage has push constants
    sed.m_vertext_stage_reflection.constants.push_back(make_push_constants("VertPC", 0, 64));

    // Fragment stage has push constants
    sed.m_fragment_stage_reflection.constants.push_back(make_push_constants("FragPC", 64, 16));

    std::vector<VkPushConstantRange> ranges;
    sed.generate_constants(ranges);

    ASSERT_EQ(ranges.size(), 2u);

    // Verify vertex push constants
    EXPECT_EQ(ranges[0].offset, 0u);
    EXPECT_EQ(ranges[0].size, 64u);
    EXPECT_TRUE(ranges[0].stageFlags & VK_SHADER_STAGE_VERTEX_BIT);

    // Verify fragment push constants
    EXPECT_EQ(ranges[1].offset, 64u);
    EXPECT_EQ(ranges[1].size, 16u);
    EXPECT_TRUE(ranges[1].stageFlags & VK_SHADER_STAGE_FRAGMENT_BIT);
}

// Test: empty reflection produces empty layouts
TEST_F(shader_reflection_test, empty_reflection_produces_empty_layouts)
{
    shader_effect_data sed(AID("empty_effect"));

    std::vector<vulkan_descriptor_set_layout_data> layouts;
    sed.generate_set_layouts(layouts);
    EXPECT_TRUE(layouts.empty());

    std::vector<VkPushConstantRange> ranges;
    sed.generate_constants(ranges);
    EXPECT_TRUE(ranges.empty());
}

// Test: dynamic UBO binding (dyn_ prefix)
TEST_F(shader_reflection_test, dynamic_ubo_binding)
{
    reflection::binding b;
    b.name = AID("dyn_camera_data");  // dyn_ prefix triggers dynamic buffer
    b.location = 0;
    b.descriptors_count = 1;
    b.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    auto ds = make_descriptor_set(0, {b});

    vulkan_descriptor_set_layout_data layout;
    vulkan_shader_reflection_utils::convert_to_ds_layout_data(ds, VK_SHADER_STAGE_VERTEX_BIT, layout);

    ASSERT_EQ(layout.bindings.size(), 1u);
    // Should be converted to dynamic uniform buffer
    EXPECT_EQ(layout.bindings[0].descriptorType, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
}

// Test: dynamic SSBO binding (dyn_ prefix)
TEST_F(shader_reflection_test, dynamic_ssbo_binding)
{
    reflection::binding b;
    b.name = AID("dyn_object_buffer");
    b.location = 0;
    b.descriptors_count = 1;
    b.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    auto ds = make_descriptor_set(0, {b});

    vulkan_descriptor_set_layout_data layout;
    vulkan_shader_reflection_utils::convert_to_ds_layout_data(ds, VK_SHADER_STAGE_VERTEX_BIT, layout);

    ASSERT_EQ(layout.bindings.size(), 1u);
    // Should be converted to dynamic storage buffer
    EXPECT_EQ(layout.bindings[0].descriptorType, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
}
