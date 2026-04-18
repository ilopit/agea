#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <format>
#include <string_view>
#include <type_traits>

// KRYGA_VULKAN_DEBUG: defined to 0 or 1 by CMake.
// When 0, the macros below compile out entirely — their arguments are never
// evaluated, so std::string construction at call sites is also stripped.
#if !defined(KRYGA_VULKAN_DEBUG)
#define KRYGA_VULKAN_DEBUG 0
#endif

namespace kryga
{
namespace render
{
namespace vk_utils
{

// Routes VkDebugUtilsMessengerEXT messages into the spdlog pipeline (ALOG_*).
// Dedupes repeated VALIDATION/PERFORMANCE warnings by messageIdNumber; errors always pass through.
// Install via vkb::InstanceBuilder::set_debug_callback(&vk_utils::debug_messenger_callback).
VKAPI_ATTR VkBool32 VKAPI_CALL
debug_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                         VkDebugUtilsMessageTypeFlagsEXT type,
                         const VkDebugUtilsMessengerCallbackDataEXT* data,
                         void* user_data);

// Clears the dedupe cache. Call on scene switch or on demand.
void
debug_reset_dedupe();

// Resolves VK_EXT_debug_utils function pointers for the instance. Call once after instance
// creation.
void
debug_init(VkInstance instance);

// --- Object naming (VK_EXT_debug_utils) -----------------------------------

void
set_debug_object_name_raw(VkDevice device,
                          VkObjectType type,
                          uint64_t handle,
                          std::string_view name);

template <typename T>
struct vk_object_type;

#define KRG_VK_OBJECT_TYPE(Handle, Enum)            \
    template <>                                     \
    struct vk_object_type<Handle>                   \
    {                                               \
        static constexpr VkObjectType value = Enum; \
    }

KRG_VK_OBJECT_TYPE(VkInstance, VK_OBJECT_TYPE_INSTANCE);
KRG_VK_OBJECT_TYPE(VkPhysicalDevice, VK_OBJECT_TYPE_PHYSICAL_DEVICE);
KRG_VK_OBJECT_TYPE(VkDevice, VK_OBJECT_TYPE_DEVICE);
KRG_VK_OBJECT_TYPE(VkQueue, VK_OBJECT_TYPE_QUEUE);
KRG_VK_OBJECT_TYPE(VkSemaphore, VK_OBJECT_TYPE_SEMAPHORE);
KRG_VK_OBJECT_TYPE(VkCommandBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER);
KRG_VK_OBJECT_TYPE(VkFence, VK_OBJECT_TYPE_FENCE);
KRG_VK_OBJECT_TYPE(VkDeviceMemory, VK_OBJECT_TYPE_DEVICE_MEMORY);
KRG_VK_OBJECT_TYPE(VkBuffer, VK_OBJECT_TYPE_BUFFER);
KRG_VK_OBJECT_TYPE(VkImage, VK_OBJECT_TYPE_IMAGE);
KRG_VK_OBJECT_TYPE(VkEvent, VK_OBJECT_TYPE_EVENT);
KRG_VK_OBJECT_TYPE(VkQueryPool, VK_OBJECT_TYPE_QUERY_POOL);
KRG_VK_OBJECT_TYPE(VkBufferView, VK_OBJECT_TYPE_BUFFER_VIEW);
KRG_VK_OBJECT_TYPE(VkImageView, VK_OBJECT_TYPE_IMAGE_VIEW);
KRG_VK_OBJECT_TYPE(VkShaderModule, VK_OBJECT_TYPE_SHADER_MODULE);
KRG_VK_OBJECT_TYPE(VkPipelineCache, VK_OBJECT_TYPE_PIPELINE_CACHE);
KRG_VK_OBJECT_TYPE(VkPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT);
KRG_VK_OBJECT_TYPE(VkRenderPass, VK_OBJECT_TYPE_RENDER_PASS);
KRG_VK_OBJECT_TYPE(VkPipeline, VK_OBJECT_TYPE_PIPELINE);
KRG_VK_OBJECT_TYPE(VkDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);
KRG_VK_OBJECT_TYPE(VkSampler, VK_OBJECT_TYPE_SAMPLER);
KRG_VK_OBJECT_TYPE(VkDescriptorPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL);
KRG_VK_OBJECT_TYPE(VkDescriptorSet, VK_OBJECT_TYPE_DESCRIPTOR_SET);
KRG_VK_OBJECT_TYPE(VkFramebuffer, VK_OBJECT_TYPE_FRAMEBUFFER);
KRG_VK_OBJECT_TYPE(VkCommandPool, VK_OBJECT_TYPE_COMMAND_POOL);
KRG_VK_OBJECT_TYPE(VkSwapchainKHR, VK_OBJECT_TYPE_SWAPCHAIN_KHR);

#undef KRG_VK_OBJECT_TYPE

template <typename Handle>
inline uint64_t
handle_to_u64(Handle h)
{
    if constexpr (std::is_pointer_v<Handle>)
    {
        return reinterpret_cast<uint64_t>(h);
    }
    else
    {
        return static_cast<uint64_t>(h);
    }
}

template <typename Handle>
inline void
set_debug_name(VkDevice device, Handle h, std::string_view name)
{
    set_debug_object_name_raw(device, vk_object_type<Handle>::value, handle_to_u64(h), name);
}

// --- Command buffer / queue labels (RenderDoc, Nsight, validation context) -

void
cmd_begin_label(VkCommandBuffer cmd,
                std::string_view name,
                float r = 1.0f,
                float g = 1.0f,
                float b = 1.0f,
                float a = 1.0f);
void
cmd_end_label(VkCommandBuffer cmd);
void
cmd_insert_label(VkCommandBuffer cmd,
                 std::string_view name,
                 float r = 1.0f,
                 float g = 1.0f,
                 float b = 1.0f,
                 float a = 1.0f);

struct scoped_cmd_label
{
    scoped_cmd_label(VkCommandBuffer cmd,
                     std::string_view name,
                     float r = 1.0f,
                     float g = 1.0f,
                     float b = 1.0f,
                     float a = 1.0f);
    ~scoped_cmd_label();
    scoped_cmd_label(const scoped_cmd_label&) = delete;
    scoped_cmd_label&
    operator=(const scoped_cmd_label&) = delete;

    VkCommandBuffer m_cmd;
};

void
queue_begin_label(VkQueue queue,
                  std::string_view name,
                  float r = 1.0f,
                  float g = 1.0f,
                  float b = 1.0f,
                  float a = 1.0f);
void
queue_end_label(VkQueue queue);

}  // namespace vk_utils
}  // namespace render
}  // namespace kryga

// --- Call-site macros (stripped in Release) -------------------------------
//
// Prefer these over calling vk_utils::set_debug_name / cmd_*_label directly.
// In Release (KRYGA_VULKAN_DEBUG=0) the entire argument list is unevaluated,
// so name-building expressions (std::string concat, to_string) compile away.

#define KRG_VK_DETAIL_CONCAT_INNER(a, b) a##b
#define KRG_VK_DETAIL_CONCAT(a, b) KRG_VK_DETAIL_CONCAT_INNER(a, b)

#if KRYGA_VULKAN_DEBUG

// For passing a debug name as a function argument (e.g. vulkan_buffer::create's debug_name param).
// In Release, the expression is unevaluated and the macro yields an empty string_view.
//
// Use KRG_VK_DBG_NAME(<any expr returning string-like>) for arbitrary expressions, or
// KRG_VK_FMT_NAME(fmt, args...) for std::format. The FMT variant hides <format> behind the
// macro, so neither the format call nor the arg evaluation runs in Release.
//
// NOTE: the returned string_view references a temporary whose lifetime ends at the enclosing
// full expression. Always pass the macro expansion directly as a function argument — do NOT
// bind to `auto v = KRG_VK_..._NAME(...)`, the view would dangle.
#define KRG_VK_DBG_NAME(name_expr) ::std::string_view{(name_expr)}
#define KRG_VK_FMT_NAME(...) ::std::string_view{::std::format(__VA_ARGS__)}

#define KRG_VK_NAME(device, handle, name_expr) \
    ::kryga::render::vk_utils::set_debug_name((device), (handle), (name_expr))

// Shorthand: name an object using std::format inline. Equivalent to
// KRG_VK_NAME(device, handle, KRG_VK_FMT_NAME(fmt, args...)).
#define KRG_VK_NAME_FMT(device, handle, ...)                 \
    ::kryga::render::vk_utils::set_debug_name((device),      \
                                              (handle),      \
                                              ::std::string_view{::std::format(__VA_ARGS__)})

#define KRG_VK_LABEL_BEGIN(cmd, name_expr) \
    ::kryga::render::vk_utils::cmd_begin_label((cmd), (name_expr))

#define KRG_VK_LABEL_END(cmd) ::kryga::render::vk_utils::cmd_end_label((cmd))

#define KRG_VK_LABEL_INSERT(cmd, name_expr) \
    ::kryga::render::vk_utils::cmd_insert_label((cmd), (name_expr))

#define KRG_VK_SCOPED_LABEL(cmd, name_expr)                                     \
    ::kryga::render::vk_utils::scoped_cmd_label KRG_VK_DETAIL_CONCAT(           \
        _krg_scoped_label_, __LINE__)((cmd), (name_expr))

#define KRG_VK_QUEUE_LABEL_BEGIN(queue, name_expr) \
    ::kryga::render::vk_utils::queue_begin_label((queue), (name_expr))

#define KRG_VK_QUEUE_LABEL_END(queue) ::kryga::render::vk_utils::queue_end_label((queue))

#else

#define KRG_VK_DBG_NAME(name_expr) ::std::string_view{}
#define KRG_VK_FMT_NAME(...) ::std::string_view{}

#define KRG_VK_NAME(device, handle, name_expr) ((void)0)
#define KRG_VK_NAME_FMT(device, handle, ...) ((void)0)
#define KRG_VK_LABEL_BEGIN(cmd, name_expr) ((void)0)
#define KRG_VK_LABEL_END(cmd) ((void)0)
#define KRG_VK_LABEL_INSERT(cmd, name_expr) ((void)0)
#define KRG_VK_SCOPED_LABEL(cmd, name_expr) ((void)0)
#define KRG_VK_QUEUE_LABEL_BEGIN(queue, name_expr) ((void)0)
#define KRG_VK_QUEUE_LABEL_END(queue) ((void)0)

#endif
