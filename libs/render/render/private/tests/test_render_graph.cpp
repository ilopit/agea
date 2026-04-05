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

// Helper: build a dynobj_layout with named fields (simulates push constant reflection)
static kryga::utils::dynobj_layout_sptr
make_pc_layout(std::initializer_list<const char*> field_names)
{
    auto layout = std::make_shared<kryga::utils::dynobj_layout>();
    uint64_t offset = 0;
    uint64_t index = 0;
    for (const char* name : field_names)
    {
        kryga::utils::dynobj_field f;
        f.id = AID(name);
        f.offset = offset;
        f.size = 8;
        f.index = index++;
        layout->get_fields_mut().push_back(std::move(f));
        offset += 8;
    }
    return layout;
}

// Helper: create a render_pass with a mock shader effect containing given push constant fields
static render_pass_sptr
make_pass_with_bda_fields(const kryga::utils::id& pass_name,
                          const kryga::utils::id& se_name,
                          std::initializer_list<const char*> field_names)
{
    auto pass = std::make_shared<render_pass>(pass_name, rg_pass_type::compute);

    reflection::shader_reflection refl;
    refl.stage = VK_SHADER_STAGE_VERTEX_BIT;
    refl.constants = reflection::push_constants{};
    refl.constants->name = AID("Constants");
    refl.constants->offset = 0;
    refl.constants->size =
        8 * static_cast<uint32_t>(std::initializer_list<const char*>(field_names).size());
    refl.constants->layout = make_pc_layout(field_names);

    auto module = std::make_shared<shader_module_data>(
        VK_NULL_HANDLE, kryga::utils::buffer{}, VK_SHADER_STAGE_VERTEX_BIT, std::move(refl));

    auto se = std::make_shared<shader_effect_data>(se_name);
    se->m_vertex_stage = module;

    pass->get_shader_effects_mut()[se_name] = se;
    return pass;
}

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
// BDA push constant validation tests
// ============================================================================

TEST(RenderGraph, bda_validation_passes_when_resources_match)
{
    vulkan_render_graph graph;
    graph.register_buffer(AID("dyn_objects"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    graph.register_buffer(AID("dyn_instance_slots"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    auto pass = make_pass_with_bda_fields(
        AID("shadow"), AID("se_shadow"), {"instance_base", "bdag_objects", "bdag_instance_slots"});
    pass->resources() = {graph.write(AID("dyn_objects"))};
    graph.add_pass(pass);

    EXPECT_TRUE(graph.compile());
}

TEST(RenderGraph, bda_validation_fails_when_resource_missing)
{
    vulkan_render_graph graph;
    graph.register_buffer(AID("dyn_objects"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    // dyn_shadow_data is NOT registered

    auto pass = make_pass_with_bda_fields(
        AID("shadow"), AID("se_shadow"), {"bdag_objects", "bdag_shadow_data"});
    pass->resources() = {graph.write(AID("dyn_objects"))};
    graph.add_pass(pass);

    EXPECT_FALSE(graph.compile());
}

TEST(RenderGraph, bda_validation_ignores_non_bdag_fields)
{
    vulkan_render_graph graph;

    // Non-bdag fields and bdaf_ fields — no graph validation needed
    auto pass = make_pass_with_bda_fields(
        AID("simple"), AID("se_simple"), {"instance_base", "material_id", "bdaf_material"});
    graph.add_pass(pass);

    EXPECT_TRUE(graph.compile());
}

TEST(RenderGraph, bda_validation_checks_all_passes)
{
    vulkan_render_graph graph;
    graph.register_buffer(AID("dyn_objects"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    graph.register_buffer(AID("dyn_camera"), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    // dyn_shadow_data is NOT registered

    // Pass 1: OK
    auto pass1 =
        make_pass_with_bda_fields(AID("main"), AID("se_main"), {"bdag_objects", "bdag_camera"});
    pass1->resources() = {graph.read(AID("dyn_objects"))};
    graph.add_pass(pass1);

    // Pass 2: has bdag_shadow_data with no matching resource
    auto pass2 = make_pass_with_bda_fields(
        AID("shadow"), AID("se_shadow"), {"bdag_objects", "bdag_shadow_data"});
    pass2->resources() = {graph.read(AID("dyn_objects"))};
    graph.add_pass(pass2);

    EXPECT_FALSE(graph.compile());
}

TEST(RenderGraph, bda_validation_passes_without_shader_effects)
{
    vulkan_render_graph graph;
    graph.register_buffer(AID("data"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    // Compute pass with no shader effects — should compile fine
    graph.add_compute_pass(AID("pass"), {graph.write(AID("data"))}, [](VkCommandBuffer) {});

    EXPECT_TRUE(graph.compile());
}

// ============================================================================
// Binding table validation tests
// ============================================================================

TEST(BindingTable, validate_resources_passes_when_all_present)
{
    vulkan_render_graph graph;
    graph.register_buffer(AID("dyn_objects"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    graph.register_buffer(AID("dyn_camera"), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    test_descriptor_layout_cache cache;
    binding_table table;
    table.add_bda(AID("dyn_objects"));
    table.add_bda(AID("dyn_camera"));
    table.finalize(cache);

    EXPECT_TRUE(table.validate_resources(graph, {}));
}

TEST(BindingTable, validate_resources_fails_when_missing)
{
    vulkan_render_graph graph;
    graph.register_buffer(AID("dyn_objects"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    // dyn_shadow_data NOT registered

    test_descriptor_layout_cache cache;
    binding_table table;
    table.add_bda(AID("dyn_objects"));
    table.add_bda(AID("dyn_shadow_data"));
    table.finalize(cache);

    EXPECT_FALSE(table.validate_resources(graph, {}));
}

TEST(BindingTable, validate_bda_bound_passes_when_all_bound)
{
    test_descriptor_layout_cache cache;
    binding_table table;
    table.add_bda(AID("dyn_objects"));
    table.add_bda(AID("dyn_camera"));
    table.finalize(cache);

    // Simulate frame: bind all declared resources
    vk_utils::vulkan_buffer dummy_buf_a;
    vk_utils::vulkan_buffer dummy_buf_b;
    table.begin_frame();
    table.bind_buffer(AID("dyn_objects"), dummy_buf_a);
    table.bind_buffer(AID("dyn_camera"), dummy_buf_b);

    EXPECT_TRUE(table.validate_bda_bound());
}

TEST(BindingTable, validate_bda_bound_fails_when_not_bound)
{
    test_descriptor_layout_cache cache;
    binding_table table;
    table.add_bda(AID("dyn_objects"));
    table.add_bda(AID("dyn_camera"));
    table.finalize(cache);

    // Simulate frame: only bind one
    vk_utils::vulkan_buffer dummy_buf;
    table.begin_frame();
    table.bind_buffer(AID("dyn_objects"), dummy_buf);
    // dyn_camera NOT bound

    EXPECT_FALSE(table.validate_bda_bound());
}

TEST(BindingTable, validate_bda_bound_fails_after_begin_frame_clears)
{
    test_descriptor_layout_cache cache;
    binding_table table;
    table.add_bda(AID("dyn_objects"));
    table.finalize(cache);

    // Frame 1: bind resource
    vk_utils::vulkan_buffer dummy_buf;
    table.begin_frame();
    table.bind_buffer(AID("dyn_objects"), dummy_buf);
    EXPECT_TRUE(table.validate_bda_bound());

    // Frame 2: begin_frame clears bindings, nothing re-bound
    table.begin_frame();
    EXPECT_FALSE(table.validate_bda_bound());
}

TEST(BindingTable, validate_resources_with_descriptor_bindings)
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

    EXPECT_TRUE(table.validate_resources(graph, {}));
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

    EXPECT_FALSE(table.validate_resources(graph, {}));
}

TEST(BindingTable, validate_resources_fails_when_not_finalized_with_descriptors)
{
    vulkan_render_graph graph;
    graph.register_buffer(AID("dyn_config"), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    binding_table table;
    table.add(
        AID("dyn_config"), 0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    // NOT finalized

    EXPECT_FALSE(table.validate_resources(graph, {}));
}

TEST(BindingTable, validate_resources_passes_bda_only_without_finalize)
{
    vulkan_render_graph graph;
    graph.register_buffer(AID("dyn_objects"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    binding_table table;
    table.add_bda(AID("dyn_objects"));
    // NOT finalized — fine for BDA-only

    EXPECT_TRUE(table.validate_resources(graph, {}));
}

TEST(BindingTable, validate_resources_mixed_bda_and_descriptors)
{
    vulkan_render_graph graph;
    graph.register_buffer(AID("dyn_camera"), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    graph.register_buffer(AID("dyn_objects"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    test_descriptor_layout_cache cache;
    binding_table table;
    table.add_bda(AID("dyn_objects"));
    table.add(
        AID("dyn_camera"), 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    table.finalize(cache);

    EXPECT_TRUE(table.validate_resources(graph, {}));
}

TEST(BindingTable, validate_resources_skips_per_material_bindings)
{
    vulkan_render_graph graph;
    // "textures" is NOT in the graph — but per_material should be skipped

    test_descriptor_layout_cache cache;
    binding_table table;
    table.add(AID("textures"),
              2,
              0,
              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              VK_SHADER_STAGE_FRAGMENT_BIT,
              binding_scope::per_material);
    table.finalize(cache);

    EXPECT_TRUE(table.validate_resources(graph, {}));
}
