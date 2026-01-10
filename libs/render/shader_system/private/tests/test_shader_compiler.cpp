#include <gtest/gtest.h>

#include <shader_compiler/shader_compiler.h>
#include <global_state/global_state.h>
#include <resource_locator/resource_locator.h>
#include <utils/file_utils.h>

using namespace kryga;
using namespace kryga::render;

class shader_compiler_test : public ::testing::Test
{
protected:
    void
    SetUp() override
    {
        m_temp_dir = glob::glob_state().get_resource_locator()->temp_dir();
    }

    void
    TearDown() override
    {
        // temp_dir_context destructor cleans up
    }

    utils::buffer
    create_shader_buffer(const std::string& source, const std::string& filename)
    {
        utils::path shader_path = m_temp_dir.folder() / filename;

        std::vector<uint8_t> data(source.begin(), source.end());
        utils::file_utils::save_file(shader_path, data);

        utils::buffer buf;
        utils::buffer::load(shader_path, buf);
        return buf;
    }

    temp_dir_context m_temp_dir;
};

TEST_F(shader_compiler_test, compile_valid_vertex_shader)
{
    const char* vert_source = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    fragNormal = inNormal;
    fragTexCoord = inTexCoord;
}
)";

    auto buf = create_shader_buffer(vert_source, "test.vert");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value()) << "Shader compilation failed";
    EXPECT_GT(result->raw_data.size(), 0u);

    // Check SPIR-V magic number (0x07230203)
    ASSERT_GE(result->raw_data.size(), 4u);
    const uint32_t* spirv = reinterpret_cast<const uint32_t*>(result->raw_data.data());
    EXPECT_EQ(spirv[0], 0x07230203u) << "Invalid SPIR-V magic number";
}

TEST_F(shader_compiler_test, compile_valid_fragment_shader)
{
    const char* frag_source = R"(
#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D texSampler;

void main() {
    vec3 color = texture(texSampler, fragTexCoord).rgb;
    outColor = vec4(color, 1.0);
}
)";

    auto buf = create_shader_buffer(frag_source, "test.frag");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value()) << "Shader compilation failed";
    EXPECT_GT(result->raw_data.size(), 0u);
}

TEST_F(shader_compiler_test, compile_valid_compute_shader)
{
    const char* comp_source = R"(
#version 450

layout(local_size_x = 16, local_size_y = 16) in;

layout(set = 0, binding = 0) buffer InputBuffer {
    float data[];
} inputData;

layout(set = 0, binding = 1) buffer OutputBuffer {
    float data[];
} outputData;

void main() {
    uint idx = gl_GlobalInvocationID.x + gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x;
    outputData.data[idx] = inputData.data[idx] * 2.0;
}
)";

    auto buf = create_shader_buffer(comp_source, "test.comp");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value()) << "Compute shader compilation failed";
    EXPECT_GT(result->raw_data.size(), 0u);
}

TEST_F(shader_compiler_test, compile_invalid_shader_returns_error)
{
    const char* invalid_source = R"(
#version 450
void main() {
    undefined_function();
}
)";

    auto buf = create_shader_buffer(invalid_source, "invalid.vert");
    auto result = shader_compiler::compile_shader(buf);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_code::compilation_failed);
}

TEST_F(shader_compiler_test, compile_shader_with_ubo)
{
    const char* vert_source = R"(
#version 450

layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 0) uniform CameraData {
    mat4 view;
    mat4 proj;
} camera;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

void main() {
    gl_Position = camera.proj * camera.view * pc.model * vec4(inPosition, 1.0);
}
)";

    auto buf = create_shader_buffer(vert_source, "ubo_test.vert");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value()) << "Shader with UBO failed to compile";
    EXPECT_GT(result->raw_data.size(), 0u);
}

TEST_F(shader_compiler_test, compile_shader_with_ssbo)
{
    const char* vert_source = R"(
#version 450

layout(location = 0) in vec3 inPosition;

struct ObjectData {
    mat4 model;
};

layout(std430, set = 0, binding = 0) readonly buffer ObjectBuffer {
    ObjectData objects[];
} objectBuffer;

void main() {
    mat4 model = objectBuffer.objects[gl_InstanceIndex].model;
    gl_Position = model * vec4(inPosition, 1.0);
}
)";

    auto buf = create_shader_buffer(vert_source, "ssbo_test.vert");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value()) << "Shader with SSBO failed to compile";
    EXPECT_GT(result->raw_data.size(), 0u);
}

TEST_F(shader_compiler_test, compile_minimal_shader)
{
    const char* minimal_vert = R"(
#version 450
void main() {
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}
)";

    auto buf = create_shader_buffer(minimal_vert, "minimal.vert");
    auto result = shader_compiler::compile_shader(buf);

    ASSERT_TRUE(result.has_value()) << "Minimal shader failed to compile";
    EXPECT_GT(result->raw_data.size(), 0u);
}

TEST_F(shader_compiler_test, compile_shader_syntax_error)
{
    const char* syntax_error = R"(
#version 450
void main() {
    vec4 pos = vec4(1.0)  // missing semicolon
    gl_Position = pos;
}
)";

    auto buf = create_shader_buffer(syntax_error, "syntax_error.vert");
    auto result = shader_compiler::compile_shader(buf);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_code::compilation_failed);
}
