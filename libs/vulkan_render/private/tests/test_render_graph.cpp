#include <gtest/gtest.h>

#include "vulkan_render/render_graph.h"

using namespace kryga::render;

// ============================================================================
// Basic construction tests
// ============================================================================

TEST(RenderGraph, default_constructed_is_empty)
{
    render_graph graph;
    ASSERT_EQ(graph.get_pass_count(), 0u);
    ASSERT_EQ(graph.get_resource_count(), 0u);
    ASSERT_FALSE(graph.is_compiled());
}

TEST(RenderGraph, create_resource)
{
    render_graph graph;
    auto& res = graph.create_resource(AID("albedo"), 1920, 1080, 37);  // VK_FORMAT_R8G8B8A8_UNORM

    ASSERT_EQ(res.name, AID("albedo"));
    ASSERT_EQ(res.width, 1920u);
    ASSERT_EQ(res.height, 1080u);
    ASSERT_EQ(res.format, 37u);
    ASSERT_FALSE(res.is_imported);
    ASSERT_EQ(graph.get_resource_count(), 1u);
}

TEST(RenderGraph, import_resource)
{
    render_graph graph;
    auto& res = graph.import_resource(AID("swapchain"));

    ASSERT_EQ(res.name, AID("swapchain"));
    EXPECT_TRUE(res.is_imported);
    ASSERT_EQ(graph.get_resource_count(), 1u);
}

TEST(RenderGraph, add_single_pass)
{
    render_graph graph;
    graph.import_resource(AID("output"));

    bool executed = false;
    graph.add_pass(AID("main_pass"), {render_graph::write(AID("output"))},
                   [&]() { executed = true; });

    ASSERT_EQ(graph.get_pass_count(), 1u);
    ASSERT_FALSE(executed);
}

// ============================================================================
// Compilation tests
// ============================================================================

TEST(RenderGraph, compile_empty_graph)
{
    render_graph graph;
    EXPECT_TRUE(graph.compile());
    EXPECT_TRUE(graph.is_compiled());
}

TEST(RenderGraph, compile_single_pass)
{
    render_graph graph;
    graph.import_resource(AID("output"));
    graph.add_pass(AID("pass"), {render_graph::write(AID("output"))}, []() {});

    EXPECT_TRUE(graph.compile());
    EXPECT_TRUE(graph.is_compiled());
}

TEST(RenderGraph, compile_fails_on_missing_resource)
{
    render_graph graph;
    // Reference non-existent resource
    graph.add_pass(AID("bad_pass"), {render_graph::read(AID("nonexistent"))}, []() {});

    ASSERT_FALSE(graph.compile());
    ASSERT_FALSE(graph.is_compiled());
    ASSERT_FALSE(graph.get_error().empty());
}

// ============================================================================
// Dependency ordering tests
// ============================================================================

TEST(RenderGraph, linear_dependency_chain)
{
    render_graph graph;

    graph.create_resource(AID("A"), 100, 100, 0);
    graph.create_resource(AID("B"), 100, 100, 0);
    graph.import_resource(AID("output"));

    // Pass order in code: 1, 2, 3
    // Dependency chain: pass1 writes A -> pass2 reads A, writes B -> pass3 reads B
    graph.add_pass(AID("pass1"), {render_graph::write(AID("A"))}, []() {});
    graph.add_pass(AID("pass2"), {render_graph::read(AID("A")), render_graph::write(AID("B"))},
                   []() {});
    graph.add_pass(AID("pass3"), {render_graph::read(AID("B")), render_graph::write(AID("output"))},
                   []() {});

    EXPECT_TRUE(graph.compile());

    // Verify order: pass1 < pass2 < pass3
    auto* p1 = graph.get_pass(AID("pass1"));
    auto* p2 = graph.get_pass(AID("pass2"));
    auto* p3 = graph.get_pass(AID("pass3"));

    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    ASSERT_NE(p3, nullptr);

    EXPECT_LT(p1->order, p2->order);
    EXPECT_LT(p2->order, p3->order);
}

TEST(RenderGraph, reverse_order_dependency)
{
    render_graph graph;

    graph.create_resource(AID("tex"), 100, 100, 0);
    graph.import_resource(AID("output"));

    // Add passes in reverse order
    graph.add_pass(AID("consumer"),
                   {render_graph::read(AID("tex")), render_graph::write(AID("output"))}, []() {});
    graph.add_pass(AID("producer"), {render_graph::write(AID("tex"))}, []() {});

    EXPECT_TRUE(graph.compile());

    auto* producer = graph.get_pass(AID("producer"));
    auto* consumer = graph.get_pass(AID("consumer"));

    // Producer must come before consumer regardless of add order
    EXPECT_LT(producer->order, consumer->order);
}

TEST(RenderGraph, independent_passes_both_execute)
{
    render_graph graph;

    graph.create_resource(AID("A"), 100, 100, 0);
    graph.create_resource(AID("B"), 100, 100, 0);

    // Two independent passes writing to different resources
    graph.add_pass(AID("pass_a"), {render_graph::write(AID("A"))}, []() {});
    graph.add_pass(AID("pass_b"), {render_graph::write(AID("B"))}, []() {});

    EXPECT_TRUE(graph.compile());
    ASSERT_EQ(graph.get_execution_order().size(), 2u);
}

TEST(RenderGraph, diamond_dependency)
{
    render_graph graph;

    //       pass1 (writes A)
    //      /            \
    //  pass2 (A->B)    pass3 (A->C)
    //      \            /
    //       pass4 (reads B,C -> output)

    graph.create_resource(AID("A"), 100, 100, 0);
    graph.create_resource(AID("B"), 100, 100, 0);
    graph.create_resource(AID("C"), 100, 100, 0);
    graph.import_resource(AID("output"));

    graph.add_pass(AID("pass1"), {render_graph::write(AID("A"))}, []() {});
    graph.add_pass(AID("pass2"), {render_graph::read(AID("A")), render_graph::write(AID("B"))},
                   []() {});
    graph.add_pass(AID("pass3"), {render_graph::read(AID("A")), render_graph::write(AID("C"))},
                   []() {});
    graph.add_pass(AID("pass4"),
                   {render_graph::read(AID("B")), render_graph::read(AID("C")),
                    render_graph::write(AID("output"))},
                   []() {});

    EXPECT_TRUE(graph.compile());

    auto* p1 = graph.get_pass(AID("pass1"));
    auto* p2 = graph.get_pass(AID("pass2"));
    auto* p3 = graph.get_pass(AID("pass3"));
    auto* p4 = graph.get_pass(AID("pass4"));

    // pass1 must be first
    EXPECT_LT(p1->order, p2->order);
    EXPECT_LT(p1->order, p3->order);

    // pass4 must be last
    EXPECT_LT(p2->order, p4->order);
    EXPECT_LT(p3->order, p4->order);
}

// ============================================================================
// Execution tests
// ============================================================================

TEST(RenderGraph, execute_runs_passes_in_order)
{
    render_graph graph;

    graph.create_resource(AID("temp"), 100, 100, 0);
    graph.import_resource(AID("output"));

    std::vector<int> execution_log;

    graph.add_pass(AID("first"), {render_graph::write(AID("temp"))},
                   [&]() { execution_log.push_back(1); });
    graph.add_pass(AID("second"),
                   {render_graph::read(AID("temp")), render_graph::write(AID("output"))},
                   [&]() { execution_log.push_back(2); });

    graph.execute();

    ASSERT_EQ(execution_log.size(), 2u);
    ASSERT_EQ(execution_log[0], 1);
    ASSERT_EQ(execution_log[1], 2);
}

TEST(RenderGraph, execute_compiles_if_needed)
{
    render_graph graph;
    graph.import_resource(AID("output"));

    bool executed = false;
    graph.add_pass(AID("pass"), {render_graph::write(AID("output"))}, [&]() { executed = true; });

    ASSERT_FALSE(graph.is_compiled());
    graph.execute();
    EXPECT_TRUE(graph.is_compiled());
    EXPECT_TRUE(executed);
}

TEST(RenderGraph, execute_throws_on_invalid_graph)
{
    render_graph graph;
    graph.add_pass(AID("bad"), {render_graph::read(AID("missing"))}, []() {});

    EXPECT_THROW(graph.execute(), std::runtime_error);
}

// ============================================================================
// Reset tests
// ============================================================================

TEST(RenderGraph, reset_clears_graph)
{
    render_graph graph;
    graph.create_resource(AID("tex"), 100, 100, 0);
    graph.add_pass(AID("pass"), {render_graph::write(AID("tex"))}, []() {});
    graph.compile();

    EXPECT_TRUE(graph.is_compiled());
    ASSERT_EQ(graph.get_pass_count(), 1u);
    ASSERT_EQ(graph.get_resource_count(), 1u);

    graph.reset();

    ASSERT_FALSE(graph.is_compiled());
    ASSERT_EQ(graph.get_pass_count(), 0u);
    ASSERT_EQ(graph.get_resource_count(), 0u);
}

TEST(RenderGraph, can_rebuild_after_reset)
{
    render_graph graph;
    graph.create_resource(AID("old"), 100, 100, 0);
    graph.compile();

    graph.reset();

    graph.create_resource(AID("new"), 200, 200, 0);
    graph.add_pass(AID("new_pass"), {render_graph::write(AID("new"))}, []() {});

    EXPECT_TRUE(graph.compile());
    ASSERT_EQ(graph.get_pass_count(), 1u);
}

// ============================================================================
// Practical usage example test
// ============================================================================

TEST(RenderGraph, deferred_rendering_pipeline)
{
    render_graph graph;

    // Resources
    graph.create_resource(AID("gbuffer_albedo"), 1920, 1080, 37);
    graph.create_resource(AID("gbuffer_normal"), 1920, 1080, 64);
    graph.create_resource(AID("gbuffer_depth"), 1920, 1080, 126);
    graph.create_resource(AID("hdr_buffer"), 1920, 1080, 109);
    graph.import_resource(AID("backbuffer"));

    std::vector<std::string> execution_order;

    // GBuffer pass
    graph.add_pass(
        AID("gbuffer"),
        {render_graph::write(AID("gbuffer_albedo")), render_graph::write(AID("gbuffer_normal")),
         render_graph::write(AID("gbuffer_depth"))},
        [&]() { execution_order.push_back("gbuffer"); });

    // Lighting pass
    graph.add_pass(
        AID("lighting"),
        {render_graph::read(AID("gbuffer_albedo")), render_graph::read(AID("gbuffer_normal")),
         render_graph::read(AID("gbuffer_depth")), render_graph::write(AID("hdr_buffer"))},
        [&]() { execution_order.push_back("lighting"); });

    // Tonemap pass
    graph.add_pass(AID("tonemap"),
                   {render_graph::read(AID("hdr_buffer")), render_graph::write(AID("backbuffer"))},
                   [&]() { execution_order.push_back("tonemap"); });

    graph.execute();

    ASSERT_EQ(execution_order.size(), 3u);
    ASSERT_EQ(execution_order[0], "gbuffer");
    ASSERT_EQ(execution_order[1], "lighting");
    ASSERT_EQ(execution_order[2], "tonemap");
}
