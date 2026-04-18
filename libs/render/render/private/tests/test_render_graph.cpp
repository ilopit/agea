#include <gtest/gtest.h>

#include "vulkan_render/vulkan_render_graph.h"
#include "vulkan_render/types/vulkan_render_pass.h"
#include "vulkan_render/types/vulkan_shader_data.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"
#include "vulkan_render/types/binding_table.h"
#include "vulkan_render/vk_descriptors.h"

using namespace kryga::render;

// Test descriptor layout cache that returns dummy handles without a VkDevice
class test_descriptor_layout_cache : public vk_utils::descriptor_layout_cache
{
public:
    VkDescriptorSetLayout
    create_descriptor_layout(VkDescriptorSetLayoutCreateInfo*) override
    {
        return reinterpret_cast<VkDescriptorSetLayout>(++m_counter);
    }

private:
    uint64_t m_counter = 0;
};

// ============================================================================
// Basic construction tests
// ============================================================================

TEST(RenderGraph, default_constructed_is_empty)
{
    vulkan_render_graph graph;
    ASSERT_EQ(graph.get_pass_count(), 0u);
    ASSERT_FALSE(graph.is_compiled());
}

// ============================================================================
// Resource registration tests
// ============================================================================

TEST(RenderGraph, register_buffer)
{
    vulkan_render_graph graph;
    graph.register_buffer(AID("vertex_data"), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    // Should be able to create a ref to it
    auto ref = graph.read(AID("vertex_data"));
    ASSERT_NE(ref.resource, nullptr);
    ASSERT_EQ(ref.resource->name, AID("vertex_data"));
    ASSERT_EQ(ref.resource->type, rg_resource_type::buffer);
    ASSERT_EQ(ref.usage, rg_access_mode::read);
}

TEST(RenderGraph, register_image)
{
    vulkan_render_graph graph;
    graph.register_image(
        AID("albedo"), 1920, 1080, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

    auto ref = graph.write(AID("albedo"));
    ASSERT_NE(ref.resource, nullptr);
    ASSERT_EQ(ref.resource->name, AID("albedo"));
    ASSERT_EQ(ref.resource->type, rg_resource_type::image);
    ASSERT_EQ(ref.resource->width, 1920u);
    ASSERT_EQ(ref.resource->height, 1080u);
    ASSERT_EQ(ref.resource->format, static_cast<uint32_t>(VK_FORMAT_R8G8B8A8_UNORM));
    ASSERT_EQ(ref.usage, rg_access_mode::write);
}

TEST(RenderGraph, import_resource)
{
    vulkan_render_graph graph;
    graph.import_resource(AID("swapchain"), rg_resource_type::image);

    auto ref = graph.read(AID("swapchain"));
    ASSERT_NE(ref.resource, nullptr);
    ASSERT_EQ(ref.resource->name, AID("swapchain"));
    EXPECT_TRUE(ref.resource->is_imported);
}

TEST(RenderGraph, read_write_returns_correct_mode)
{
    vulkan_render_graph graph;
    graph.register_buffer(AID("data"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    auto read_ref = graph.read(AID("data"));
    auto write_ref = graph.write(AID("data"));
    auto rw_ref = graph.read_write(AID("data"));

    ASSERT_EQ(read_ref.usage, rg_access_mode::read);
    ASSERT_EQ(write_ref.usage, rg_access_mode::write);
    ASSERT_EQ(rw_ref.usage, rg_access_mode::read_write);

    // All should point to the same resource
    ASSERT_EQ(read_ref.resource, write_ref.resource);
    ASSERT_EQ(write_ref.resource, rw_ref.resource);
}

TEST(RenderGraph, unknown_resource_returns_null)
{
    vulkan_render_graph graph;

    auto ref = graph.read(AID("nonexistent"));
    ASSERT_EQ(ref.resource, nullptr);
}

// ============================================================================
// Pass registration tests
// ============================================================================

TEST(RenderGraph, add_compute_pass)
{
    vulkan_render_graph graph;
    graph.register_buffer(AID("data"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    auto pass = graph.add_compute_pass(
        AID("compute"), {graph.read_write(AID("data"))}, [](VkCommandBuffer) {});

    ASSERT_NE(pass, nullptr);
    ASSERT_EQ(graph.get_pass_count(), 1u);
}

TEST(RenderGraph, add_transfer_pass)
{
    vulkan_render_graph graph;
    graph.register_buffer(AID("src"), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    graph.register_buffer(AID("dst"), VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    auto pass = graph.add_transfer_pass(
        AID("copy"), {graph.read(AID("src")), graph.write(AID("dst"))}, [](VkCommandBuffer) {});

    ASSERT_NE(pass, nullptr);
    ASSERT_EQ(graph.get_pass_count(), 1u);
}

TEST(RenderGraph, get_pass_by_name)
{
    vulkan_render_graph graph;
    graph.register_buffer(AID("buf"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    graph.add_compute_pass(AID("my_pass"), {graph.read(AID("buf"))}, [](VkCommandBuffer) {});

    auto* found = graph.get_pass(AID("my_pass"));
    ASSERT_NE(found, nullptr);
    ASSERT_EQ(found->name(), AID("my_pass"));

    auto* not_found = graph.get_pass(AID("other"));
    ASSERT_EQ(not_found, nullptr);
}

// ============================================================================
// Compilation tests
// ============================================================================

TEST(RenderGraph, compile_empty_graph)
{
    vulkan_render_graph graph;
    EXPECT_TRUE(graph.compile());
    EXPECT_TRUE(graph.is_compiled());
}

TEST(RenderGraph, compile_single_pass)
{
    vulkan_render_graph graph;
    graph.register_buffer(AID("output"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    graph.add_compute_pass(AID("pass"), {graph.write(AID("output"))}, [](VkCommandBuffer) {});

    EXPECT_TRUE(graph.compile());
    EXPECT_TRUE(graph.is_compiled());
    ASSERT_EQ(graph.get_execution_order().size(), 1u);
}

TEST(RenderGraph, compile_multiple_passes)
{
    vulkan_render_graph graph;
    graph.register_buffer(AID("a"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    graph.register_buffer(AID("b"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    graph.add_compute_pass(AID("pass1"), {graph.write(AID("a"))}, [](VkCommandBuffer) {});
    graph.add_compute_pass(
        AID("pass2"), {graph.read(AID("a")), graph.write(AID("b"))}, [](VkCommandBuffer) {});

    EXPECT_TRUE(graph.compile());
    ASSERT_EQ(graph.get_execution_order().size(), 2u);
}

// ============================================================================
// Dependency ordering tests
// ============================================================================

TEST(RenderGraph, linear_dependency_chain)
{
    vulkan_render_graph graph;

    graph.register_buffer(AID("A"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    graph.register_buffer(AID("B"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    graph.register_buffer(AID("C"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    // Pass order in code: 1, 2, 3
    // Dependency chain: pass1 writes A -> pass2 reads A, writes B -> pass3 reads B
    graph.add_compute_pass(AID("pass1"), {graph.write(AID("A"))}, [](VkCommandBuffer) {});
    graph.add_compute_pass(
        AID("pass2"), {graph.read(AID("A")), graph.write(AID("B"))}, [](VkCommandBuffer) {});
    graph.add_compute_pass(
        AID("pass3"), {graph.read(AID("B")), graph.write(AID("C"))}, [](VkCommandBuffer) {});

    EXPECT_TRUE(graph.compile());

    auto* p1 = graph.get_pass(AID("pass1"));
    auto* p2 = graph.get_pass(AID("pass2"));
    auto* p3 = graph.get_pass(AID("pass3"));

    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    ASSERT_NE(p3, nullptr);

    EXPECT_LT(p1->order(), p2->order());
    EXPECT_LT(p2->order(), p3->order());
}

TEST(RenderGraph, independent_passes_both_compile)
{
    vulkan_render_graph graph;

    graph.register_buffer(AID("A"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    graph.register_buffer(AID("B"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    // Two independent passes writing to different resources
    graph.add_compute_pass(AID("pass_a"), {graph.write(AID("A"))}, [](VkCommandBuffer) {});
    graph.add_compute_pass(AID("pass_b"), {graph.write(AID("B"))}, [](VkCommandBuffer) {});

    EXPECT_TRUE(graph.compile());
    ASSERT_EQ(graph.get_execution_order().size(), 2u);
}

TEST(RenderGraph, diamond_dependency)
{
    vulkan_render_graph graph;

    //       pass1 (writes A)
    //      /            \
    //  pass2 (A->B)    pass3 (A->C)
    //      \            /
    //       pass4 (reads B,C -> D)

    graph.register_buffer(AID("A"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    graph.register_buffer(AID("B"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    graph.register_buffer(AID("C"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    graph.register_buffer(AID("D"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    graph.add_compute_pass(AID("pass1"), {graph.write(AID("A"))}, [](VkCommandBuffer) {});
    graph.add_compute_pass(
        AID("pass2"), {graph.read(AID("A")), graph.write(AID("B"))}, [](VkCommandBuffer) {});
    graph.add_compute_pass(
        AID("pass3"), {graph.read(AID("A")), graph.write(AID("C"))}, [](VkCommandBuffer) {});
    graph.add_compute_pass(AID("pass4"),
                           {graph.read(AID("B")), graph.read(AID("C")), graph.write(AID("D"))},
                           [](VkCommandBuffer) {});

    EXPECT_TRUE(graph.compile());

    auto* p1 = graph.get_pass(AID("pass1"));
    auto* p2 = graph.get_pass(AID("pass2"));
    auto* p3 = graph.get_pass(AID("pass3"));
    auto* p4 = graph.get_pass(AID("pass4"));

    // pass1 must be first
    EXPECT_LT(p1->order(), p2->order());
    EXPECT_LT(p1->order(), p3->order());

    // pass4 must be last
    EXPECT_LT(p2->order(), p4->order());
    EXPECT_LT(p3->order(), p4->order());
}

// ============================================================================
// Reset tests
// ============================================================================

TEST(RenderGraph, reset_clears_graph)
{
    vulkan_render_graph graph;
    graph.register_buffer(AID("buf"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    graph.add_compute_pass(AID("pass"), {graph.write(AID("buf"))}, [](VkCommandBuffer) {});
    graph.compile();

    EXPECT_TRUE(graph.is_compiled());
    ASSERT_EQ(graph.get_pass_count(), 1u);

    graph.reset();

    ASSERT_FALSE(graph.is_compiled());
    ASSERT_EQ(graph.get_pass_count(), 0u);
}

TEST(RenderGraph, can_rebuild_after_reset)
{
    vulkan_render_graph graph;
    graph.register_buffer(AID("old"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    graph.compile();

    graph.reset();

    graph.register_buffer(AID("new"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    graph.add_compute_pass(AID("new_pass"), {graph.write(AID("new"))}, [](VkCommandBuffer) {});

    EXPECT_TRUE(graph.compile());
    ASSERT_EQ(graph.get_pass_count(), 1u);
}

// ============================================================================
// Practical usage example test
// ============================================================================

TEST(RenderGraph, deferred_rendering_pipeline_structure)
{
    vulkan_render_graph graph;

    // Resources
    graph.register_image(AID("gbuffer_albedo"),
                         1920,
                         1080,
                         VK_FORMAT_R8G8B8A8_UNORM,
                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    graph.register_image(AID("gbuffer_normal"),
                         1920,
                         1080,
                         VK_FORMAT_R16G16B16A16_SFLOAT,
                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    graph.register_image(AID("gbuffer_depth"),
                         1920,
                         1080,
                         VK_FORMAT_D32_SFLOAT,
                         VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    graph.register_image(AID("hdr_buffer"),
                         1920,
                         1080,
                         VK_FORMAT_R16G16B16A16_SFLOAT,
                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    graph.import_resource(AID("backbuffer"), rg_resource_type::image);

    // GBuffer pass (compute for testing purposes - real would be graphics)
    graph.add_compute_pass(AID("gbuffer"),
                           {graph.write(AID("gbuffer_albedo")),
                            graph.write(AID("gbuffer_normal")),
                            graph.write(AID("gbuffer_depth"))},
                           [](VkCommandBuffer) {});

    // Lighting pass
    graph.add_compute_pass(AID("lighting"),
                           {graph.read(AID("gbuffer_albedo")),
                            graph.read(AID("gbuffer_normal")),
                            graph.read(AID("gbuffer_depth")),
                            graph.write(AID("hdr_buffer"))},
                           [](VkCommandBuffer) {});

    // Tonemap pass
    graph.add_compute_pass(AID("tonemap"),
                           {graph.read(AID("hdr_buffer")), graph.write(AID("backbuffer"))},
                           [](VkCommandBuffer) {});

    EXPECT_TRUE(graph.compile());
    ASSERT_EQ(graph.get_execution_order().size(), 3u);

    // Verify ordering
    auto* gbuffer = graph.get_pass(AID("gbuffer"));
    auto* lighting = graph.get_pass(AID("lighting"));
    auto* tonemap = graph.get_pass(AID("tonemap"));

    EXPECT_LT(gbuffer->order(), lighting->order());
    EXPECT_LT(lighting->order(), tonemap->order());
}

// ============================================================================
// Binding table validation tests
// ============================================================================

TEST(BindingTable, validate_resources_passes_when_all_present)
{
    vulkan_render_graph graph;
    graph.register_buffer(AID("dyn_config"), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    graph.register_buffer(AID("dyn_lights"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    test_descriptor_layout_cache cache;
    binding_table table;
    table.add(
        AID("dyn_config"), 0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    table.add(
        AID("dyn_lights"), 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    table.finalize(cache);

    EXPECT_TRUE(table.validate_resources(graph));
}

TEST(BindingTable, validate_resources_fails_for_missing_descriptor_binding)
{
    vulkan_render_graph graph;
    graph.register_buffer(AID("dyn_config"), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    // dyn_lights NOT registered

    test_descriptor_layout_cache cache;
    binding_table table;
    table.add(
        AID("dyn_config"), 0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    table.add(
        AID("dyn_lights"), 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    table.finalize(cache);

    EXPECT_FALSE(table.validate_resources(graph));
}

TEST(BindingTable, validate_resources_skips_per_material_bindings)
{
    vulkan_render_graph graph;
    // Register only the per_pass resource — per_material binding (e.g., bindless textures)
    // shouldn't need to live in the render graph.
    graph.register_buffer(AID("dyn_config"), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    test_descriptor_layout_cache cache;
    binding_table table;
    table.add(
        AID("dyn_config"), 0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    table.add(AID("bindless_textures"),
              2,
              1,
              VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
              VK_SHADER_STAGE_FRAGMENT_BIT,
              binding_scope::per_material);
    table.finalize(cache);

    EXPECT_TRUE(table.validate_resources(graph));
}
