#include "shader_compiler_test_fixture.h"

using namespace kryga;
using namespace kryga::render;
using namespace kryga::render::test;

TEST_F(shader_compiler_test, reflection_push_constants_single_mat4)
{
    const char* vert_source = R"(
#version 450

layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

void main() {
    gl_Position = pc.mvp * vec4(1.0);
}
)";

    auto buf = create_shader_buffer(vert_source, "pc_mat4.vert");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value());

    const auto& refl = result->reflection;
    print_reflection(refl);

    EXPECT_EQ(refl.constants->offset, 0u);
    EXPECT_EQ(refl.constants->size, 64u);  // mat4 = 4x4x4 = 64 bytes
}

TEST_F(shader_compiler_test, reflection_push_constants_multiple_fields)
{
    const char* vert_source = R"(
#version 450

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
    float time;
} pc;

void main() {
    gl_Position = pc.model * vec4(1.0) + pc.color * pc.time;
}
)";

    auto buf = create_shader_buffer(vert_source, "pc_multi.vert");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value());

    const auto& refl = result->reflection;
    print_reflection(refl);

    EXPECT_EQ(refl.constants->offset, 0u);
    // mat4(64) + vec4(16) + float(4) + padding(12) = 96 bytes (or 84 without padding)
    EXPECT_GE(refl.constants->size, 84u);
}

TEST_F(shader_compiler_test, reflection_push_constants_multiple_fields_in_struct)
{
    const char* vert_source = R"(
#version 450

struct PushConstantsData
{
    mat4 model;
    vec4 color;
    float time;
};


layout(push_constant) uniform PushConstants {
    PushConstantsData obj;
} pc;

void main() {
    gl_Position = pc.obj.model * vec4(1.0) + pc.obj.color * pc.obj.time;
}
)";

    auto buf = create_shader_buffer(vert_source, "pc_multi.vert");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value());

    const auto& refl = result->reflection;
    print_reflection(refl);

    EXPECT_EQ(refl.constants->offset, 0u);
    // mat4(64) + vec4(16) + float(4) + padding(12) = 96 bytes (or 84 without padding)
    EXPECT_GE(refl.constants->size, 84u);
}

TEST_F(shader_compiler_test, reflection_descriptor_set_ubo)
{
    const char* vert_source = R"(
#version 450

layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 0) uniform CameraData {
    mat4 view;
    mat4 proj;
} camera;

void main() {
    gl_Position = camera.proj * camera.view * vec4(inPosition, 1.0);
}
)";

    auto buf = create_shader_buffer(vert_source, "ubo_refl.vert");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value());

    const auto& refl = result->reflection;
    print_reflection(refl);
    ASSERT_GE(refl.descriptors.size(), 1u);

    // Find set 0 using helper
    auto* set0 = refl.find_set(0);
    ASSERT_NE(set0, nullptr) << "Descriptor set 0 not found";
    ASSERT_GE(set0->bindings.size(), 1u);

    // Find binding 0 using helper
    auto* binding0 = refl.find_binding(0, 0);
    ASSERT_NE(binding0, nullptr) << "Binding 0 not found in set 0";
    EXPECT_EQ(static_cast<int>(binding0->type),
              static_cast<int>(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER));
}

TEST_F(shader_compiler_test, reflection_descriptor_set_ssbo)
{
    const char* vert_source = R"(
#version 450

layout(location = 0) in vec3 inPosition;

struct ObjectData {
    mat4 model;
};

layout(std430, set = 1, binding = 2) readonly buffer ObjectBuffer {
    ObjectData objects[];
} objectBuffer;

void main() {
    mat4 model = objectBuffer.objects[gl_InstanceIndex].model;
    gl_Position = model * vec4(inPosition, 1.0);
}
)";

    auto buf = create_shader_buffer(vert_source, "ssbo_refl.vert");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value());

    const auto& refl = result->reflection;
    print_reflection(refl);
    ASSERT_GE(refl.descriptors.size(), 1u);

    // Find set 1 and binding 2 using helpers
    auto* set1 = refl.find_set(1);
    ASSERT_NE(set1, nullptr) << "Descriptor set 1 not found";
    ASSERT_GE(set1->bindings.size(), 1u);

    auto* binding2 = refl.find_binding(1, 2);
    ASSERT_NE(binding2, nullptr) << "Binding 2 not found in set 1";
    EXPECT_EQ(static_cast<int>(binding2->type),
              static_cast<int>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
}

TEST_F(shader_compiler_test, reflection_descriptor_set_ssbo_array)
{
    const char* vert_source = R"(
#version 450

layout(location = 0) in vec3 inPosition;

layout(std430, set = 1, binding = 2) readonly buffer ObjectBuffer {
    float objects[];
} objectBuffer;

void main() {
    gl_Position = vec4(objectBuffer.objects[gl_InstanceIndex]);
}
)";

    auto buf = create_shader_buffer(vert_source, "ssbo_refl.vert");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value());

    const auto& refl = result->reflection;
    print_reflection(refl);
    ASSERT_GE(refl.descriptors.size(), 1u);

    // Find set 1 and binding 2 using helpers
    auto* set1 = refl.find_set(1);
    ASSERT_NE(set1, nullptr) << "Descriptor set 1 not found";
    ASSERT_GE(set1->bindings.size(), 1u);

    auto* binding2 = refl.find_binding(1, 2);
    ASSERT_NE(binding2, nullptr) << "Binding 2 not found in set 1";
    EXPECT_EQ(static_cast<int>(binding2->type),
              static_cast<int>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
}

TEST_F(shader_compiler_test, reflection_descriptor_set_sampler)
{
    const char* frag_source = R"(
#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D albedoTex;
layout(set = 2, binding = 1) uniform sampler2D normalTex;

void main() {
    vec4 albedo = texture(albedoTex, fragTexCoord);
    vec4 normal = texture(normalTex, fragTexCoord);
    outColor = albedo + normal * 0.01;
}
)";

    auto buf = create_shader_buffer(frag_source, "sampler_refl.frag");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value());

    const auto& refl = result->reflection;
    print_reflection(refl);
    // Find set 2 using helper
    auto* set2 = refl.find_set(2);
    ASSERT_NE(set2, nullptr) << "Descriptor set 2 not found";
    EXPECT_EQ(set2->bindings.size(), 2u);

    for (const auto& b : set2->bindings)
    {
        EXPECT_EQ(static_cast<int>(b.type),
                  static_cast<int>(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER));
    }
}

TEST_F(shader_compiler_test, reflection_input_interface_vertex_shader)
{
    const char* vert_source = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inColor;

layout(location = 0) out vec3 fragNormal;

void main() {
    gl_Position = vec4(inPosition, 1.0);
    fragNormal = inNormal + vec3(inTexCoord, 0.0) + inColor.xyz;
}
)";

    auto buf = create_shader_buffer(vert_source, "input_refl.vert");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value());

    const auto& refl = result->reflection;
    print_reflection(refl);
    ASSERT_NE(refl.input_interface.layout, nullptr);

    // Should have 4 input variables (layout is wrapped, access inner layout)
    const auto& layout = refl.input_interface.layout;
    ASSERT_EQ(layout->get_fields().size(), 1u);
    ASSERT_NE(layout->get_fields()[0].sub_field_layout, nullptr);
    EXPECT_EQ(layout->get_fields()[0].sub_field_layout->get_fields().size(), 4u);
}

TEST_F(shader_compiler_test, reflection_output_interface_vertex_shader)
{
    const char* vert_source = R"(
#version 450

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 fragPosition;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

void main() {
    gl_Position = vec4(inPosition, 1.0);
    fragPosition = inPosition;
    fragNormal = vec3(0.0, 1.0, 0.0);
    fragTexCoord = vec2(0.0);
}
)";

    auto buf = create_shader_buffer(vert_source, "output_refl.vert");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value());

    const auto& refl = result->reflection;
    print_reflection(refl);
    ASSERT_NE(refl.output_interface.layout, nullptr);

    // Should have 3 output variables (layout is wrapped, access inner layout)
    const auto& layout = refl.output_interface.layout;
    ASSERT_EQ(layout->get_fields().size(), 1u);
    ASSERT_NE(layout->get_fields()[0].sub_field_layout, nullptr);
    EXPECT_EQ(layout->get_fields()[0].sub_field_layout->get_fields().size(), 3u);
}

TEST_F(shader_compiler_test, reflection_multiple_descriptor_sets)
{
    const char* frag_source = R"(
#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform GlobalData {
    float time;
} globals;

layout(set = 1, binding = 0) uniform MaterialData {
    vec4 baseColor;
} material;

layout(set = 2, binding = 0) uniform sampler2D tex;

void main() {
    outColor = material.baseColor * texture(tex, fragTexCoord) * globals.time;
}
)";

    auto buf = create_shader_buffer(frag_source, "multi_set_refl.frag");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value());

    const auto& refl = result->reflection;
    print_reflection(refl);

    // Should have 3 descriptor sets (0, 1, 2) - use helpers
    EXPECT_GE(refl.descriptors.size(), 3u);

    EXPECT_NE(refl.find_set(0), nullptr) << "Set 0 not found";
    EXPECT_NE(refl.find_set(1), nullptr) << "Set 1 not found";
    EXPECT_NE(refl.find_set(2), nullptr) << "Set 2 not found";
}

TEST_F(shader_compiler_test, reflection_compute_shader_bindings)
{
    const char* comp_source = R"(
#version 450

layout(local_size_x = 64) in;

layout(set = 0, binding = 0) readonly buffer InputBuffer {
    float data[];
} inputData;

layout(set = 0, binding = 1) writeonly buffer OutputBuffer {
    float data[];
} outputData;

layout(push_constant) uniform PushConstants {
    uint count;
} pc;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < pc.count) {
        outputData.data[idx] = inputData.data[idx] * 2.0;
    }
}
)";

    auto buf = create_shader_buffer(comp_source, "compute_refl.comp");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value());

    const auto& refl = result->reflection;
    print_reflection(refl);

    // Check push constants
    EXPECT_EQ(refl.constants->size, 4u);  // uint = 4 bytes

    // Check descriptor set 0 with 2 bindings using helper
    auto* set0 = refl.find_set(0);
    ASSERT_NE(set0, nullptr);
    EXPECT_EQ(set0->bindings.size(), 2u);

    for (const auto& b : set0->bindings)
    {
        EXPECT_EQ(static_cast<int>(b.type), static_cast<int>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
    }
}

TEST_F(shader_compiler_test, reflection_no_descriptors_minimal_shader)
{
    const char* vert_source = R"(
#version 450

layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = vec4(inPosition, 1.0);
}
)";

    auto buf = create_shader_buffer(vert_source, "no_desc.vert");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value());

    const auto& refl = result->reflection;
    print_reflection(refl);

    // No push constants
    ASSERT_TRUE(!refl.constants);

    // No descriptor sets (or empty sets)
    for (const auto& ds : refl.descriptors)
    {
        EXPECT_EQ(ds.bindings.size(), 0u);
    }

    // Should have 1 input (layout is wrapped, access inner layout)
    ASSERT_NE(refl.input_interface.layout, nullptr);
    ASSERT_EQ(refl.input_interface.layout->get_fields().size(), 1u);
    ASSERT_NE(refl.input_interface.layout->get_fields()[0].sub_field_layout, nullptr);
    EXPECT_EQ(refl.input_interface.layout->get_fields()[0].sub_field_layout->get_fields().size(),
              1u);
}

TEST_F(shader_compiler_test, reflection_rejects_unnamed_push_constant)
{
    const char* vert_source = R"(
#version 450

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
    float time;
};

void main() {
    gl_Position = model * vec4(1.0) + color * time;
}
)";

    auto buf = create_shader_buffer(vert_source, "unnamed_pc.vert");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_FALSE(result.has_value()) << "Should reject push constant without instance name";
}

TEST_F(shader_compiler_test, reflection_rejects_unnamed_ubo)
{
    const char* vert_source = R"(
#version 450

layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 0) uniform CameraData {
    mat4 view;
    mat4 proj;
};

void main() {
    gl_Position = proj * view * vec4(inPosition, 1.0);
}
)";

    auto buf = create_shader_buffer(vert_source, "unnamed_ubo.vert");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_FALSE(result.has_value()) << "Should reject UBO without instance name";
}

TEST_F(shader_compiler_test, reflection_rejects_unnamed_ssbo)
{
    const char* vert_source = R"(
#version 450

layout(location = 0) in vec3 inPosition;

struct ObjectData {
    mat4 model;
};

layout(std430, set = 1, binding = 0) readonly buffer ObjectBuffer {
    ObjectData objects[];
};

void main() {
    mat4 model = objects[gl_InstanceIndex].model;
    gl_Position = model * vec4(inPosition, 1.0);
}
)";

    auto buf = create_shader_buffer(vert_source, "unnamed_ssbo.vert");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_FALSE(result.has_value()) << "Should reject SSBO without instance name";
}

// ============================================================================
// Specialization Constants
// ============================================================================

TEST_F(shader_compiler_test, reflection_spec_constants_single_bool)
{
    const char* vert_source = R"(
#version 450

layout(constant_id = 0) const bool ENABLE_LIGHTMAP = false;

void main() {
    if (ENABLE_LIGHTMAP) {
        gl_Position = vec4(1.0);
    } else {
        gl_Position = vec4(0.0);
    }
}
)";

    auto buf = create_shader_buffer(vert_source, "spec_single.vert");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value());

    const auto& refl = result->reflection;
    ASSERT_EQ(refl.spec_constants.size(), 1u);
    EXPECT_EQ(refl.spec_constants[0].name, "ENABLE_LIGHTMAP");
    EXPECT_EQ(refl.spec_constants[0].constant_id, 0u);
}

TEST_F(shader_compiler_test, reflection_spec_constants_multiple)
{
    const char* frag_source = R"(
#version 450

layout(constant_id = 0) const bool ENABLE_LIGHTMAP = false;
layout(constant_id = 1) const bool ENABLE_FOG = false;
layout(constant_id = 2) const int SHADOW_QUALITY = 0;

layout(location = 0) out vec4 out_color;

void main() {
    vec4 c = vec4(1.0);
    if (ENABLE_LIGHTMAP) c *= 0.5;
    if (ENABLE_FOG) c *= 0.8;
    c *= float(SHADOW_QUALITY);
    out_color = c;
}
)";

    auto buf = create_shader_buffer(frag_source, "spec_multi.frag");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value());

    const auto& refl = result->reflection;
    ASSERT_EQ(refl.spec_constants.size(), 3u);

    // Verify we can find each by name
    auto* lm = refl.find_spec_constant("ENABLE_LIGHTMAP");
    auto* fog = refl.find_spec_constant("ENABLE_FOG");
    auto* sq = refl.find_spec_constant("SHADOW_QUALITY");

    ASSERT_NE(lm, nullptr);
    ASSERT_NE(fog, nullptr);
    ASSERT_NE(sq, nullptr);

    EXPECT_EQ(lm->constant_id, 0u);
    EXPECT_EQ(fog->constant_id, 1u);
    EXPECT_EQ(sq->constant_id, 2u);
}

TEST_F(shader_compiler_test, reflection_spec_constants_none)
{
    const char* vert_source = R"(
#version 450

void main() {
    gl_Position = vec4(0.0);
}
)";

    auto buf = create_shader_buffer(vert_source, "spec_none.vert");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value());

    const auto& refl = result->reflection;
    EXPECT_TRUE(refl.spec_constants.empty());
    EXPECT_EQ(refl.find_spec_constant("ANYTHING"), nullptr);
}

TEST_F(shader_compiler_test, reflection_spec_constants_find_by_name_miss)
{
    const char* vert_source = R"(
#version 450

layout(constant_id = 0) const bool ENABLE_LIGHTMAP = false;

void main() {
    gl_Position = ENABLE_LIGHTMAP ? vec4(1.0) : vec4(0.0);
}
)";

    auto buf = create_shader_buffer(vert_source, "spec_miss.vert");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value());

    const auto& refl = result->reflection;
    EXPECT_NE(refl.find_spec_constant("ENABLE_LIGHTMAP"), nullptr);
    EXPECT_EQ(refl.find_spec_constant("ENABLE_FOG"), nullptr);
    EXPECT_EQ(refl.find_spec_constant(""), nullptr);
    EXPECT_EQ(refl.find_spec_constant("enable_lightmap"), nullptr);  // case sensitive
}

TEST_F(shader_compiler_test, reflection_spec_constants_non_sequential_ids)
{
    const char* frag_source = R"(
#version 450

layout(constant_id = 5) const bool FLAG_A = false;
layout(constant_id = 10) const bool FLAG_B = true;

layout(location = 0) out vec4 out_color;

void main() {
    out_color = vec4(float(FLAG_A), float(FLAG_B), 0.0, 1.0);
}
)";

    auto buf = create_shader_buffer(frag_source, "spec_nonseq.frag");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value());

    const auto& refl = result->reflection;
    ASSERT_EQ(refl.spec_constants.size(), 2u);

    auto* a = refl.find_spec_constant("FLAG_A");
    auto* b = refl.find_spec_constant("FLAG_B");

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    EXPECT_EQ(a->constant_id, 5u);
    EXPECT_EQ(b->constant_id, 10u);
}
