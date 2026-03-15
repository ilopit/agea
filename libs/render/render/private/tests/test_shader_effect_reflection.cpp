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
    make_ubo_binding(const char* name, uint32_t bind_idx)
    {
        reflection::binding b;
        b.name = AID(name);
        b.binding_index = bind_idx;
        b.descriptors_count = 1;
        b.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        return b;
    }

    // Helper to create an SSBO binding
    reflection::binding
    make_ssbo_binding(const char* name, uint32_t bind_idx)
    {
        reflection::binding b;
        b.name = AID(name);
        b.binding_index = bind_idx;
        b.descriptors_count = 1;
        b.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        return b;
    }

    // Helper to create a descriptor set
    reflection::descriptor_set
    make_descriptor_set(uint32_t set_idx, std::vector<reflection::binding> binds)
    {
        reflection::descriptor_set ds;
        ds.set_index = set_idx;
        ds.bindings = std::move(binds);
        return ds;
    }

    // Helper to create push constants
    reflection::push_constants
    make_push_constants(const char* name, uint32_t offset, uint32_t size)
    {
        reflection::push_constants pc;
        pc.name = AID(name);
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
    vulkan_shader_reflection_utils::convert_to_ds_layout_data(ds, VK_SHADER_STAGE_FRAGMENT_BIT,
                                                              layout);

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
    vulkan_shader_reflection_utils::convert_to_ds_layout_data(ds, VK_SHADER_STAGE_VERTEX_BIT,
                                                              layout);

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
    vulkan_shader_reflection_utils::convert_to_ds_layout_data(ds, VK_SHADER_STAGE_VERTEX_BIT,
                                                              layout);

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
    vulkan_shader_reflection_utils::convert_to_ds_layout_data(ds, VK_SHADER_STAGE_FRAGMENT_BIT,
                                                              layout);

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

// Note: Tests for generate_set_layouts and generate_constants from shader_effect_data
// require setting up shader_module_data which needs VkShaderModule handles.
// These are tested through integration tests instead.

// Test: dynamic UBO binding (dyn_ prefix)
TEST_F(shader_reflection_test, dynamic_ubo_binding)
{
    reflection::binding b;
    b.name = AID("dyn_camera_data");  // dyn_ prefix triggers dynamic buffer
    b.binding_index = 0;
    b.descriptors_count = 1;
    b.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    auto ds = make_descriptor_set(0, {b});

    vulkan_descriptor_set_layout_data layout;
    vulkan_shader_reflection_utils::convert_to_ds_layout_data(ds, VK_SHADER_STAGE_VERTEX_BIT,
                                                              layout);

    ASSERT_EQ(layout.bindings.size(), 1u);
    // Should be converted to dynamic uniform buffer
    EXPECT_EQ(layout.bindings[0].descriptorType, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
}

// Test: dynamic SSBO binding (dyn_ prefix)
TEST_F(shader_reflection_test, dynamic_ssbo_binding)
{
    reflection::binding b;
    b.name = AID("dyn_object_buffer");
    b.binding_index = 0;
    b.descriptors_count = 1;
    b.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    auto ds = make_descriptor_set(0, {b});

    vulkan_descriptor_set_layout_data layout;
    vulkan_shader_reflection_utils::convert_to_ds_layout_data(ds, VK_SHADER_STAGE_VERTEX_BIT,
                                                              layout);

    ASSERT_EQ(layout.bindings.size(), 1u);
    // Should be converted to dynamic storage buffer
    EXPECT_EQ(layout.bindings[0].descriptorType, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
}
