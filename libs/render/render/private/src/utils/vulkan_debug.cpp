#include "vulkan_render/utils/vulkan_debug.h"

#include <utils/kryga_log.h>

#include <cstring>
#include <mutex>
#include <string>
#include <unordered_set>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace kryga
{
namespace render
{
namespace vk_utils
{

namespace
{

struct dedupe_state
{
    std::mutex mtx;
    std::unordered_set<uint64_t> seen;
};

dedupe_state&
dedupe()
{
    static dedupe_state s;
    return s;
}

uint64_t
fnv1a(const char* s, size_t n)
{
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n && s[i]; ++i)
    {
        h ^= static_cast<uint8_t>(s[i]);
        h *= 0x100000001b3ULL;
    }
    return h;
}

bool
already_seen(int32_t id, const char* msg)
{
    // Combine id + prefix of message so different VUIDs on same id still log separately.
    uint64_t key = static_cast<uint32_t>(id);
    if (msg)
    {
        key = (key << 32) ^ fnv1a(msg, 192);
    }
    auto& d = dedupe();
    std::lock_guard<std::mutex> lk(d.mtx);
    return !d.seen.insert(key).second;
}

const char*
object_type_str(VkObjectType t)
{
    switch (t)
    {
    case VK_OBJECT_TYPE_INSTANCE:
        return "Instance";
    case VK_OBJECT_TYPE_PHYSICAL_DEVICE:
        return "PhysicalDevice";
    case VK_OBJECT_TYPE_DEVICE:
        return "Device";
    case VK_OBJECT_TYPE_QUEUE:
        return "Queue";
    case VK_OBJECT_TYPE_SEMAPHORE:
        return "Semaphore";
    case VK_OBJECT_TYPE_COMMAND_BUFFER:
        return "CommandBuffer";
    case VK_OBJECT_TYPE_FENCE:
        return "Fence";
    case VK_OBJECT_TYPE_DEVICE_MEMORY:
        return "DeviceMemory";
    case VK_OBJECT_TYPE_BUFFER:
        return "Buffer";
    case VK_OBJECT_TYPE_IMAGE:
        return "Image";
    case VK_OBJECT_TYPE_EVENT:
        return "Event";
    case VK_OBJECT_TYPE_QUERY_POOL:
        return "QueryPool";
    case VK_OBJECT_TYPE_BUFFER_VIEW:
        return "BufferView";
    case VK_OBJECT_TYPE_IMAGE_VIEW:
        return "ImageView";
    case VK_OBJECT_TYPE_SHADER_MODULE:
        return "ShaderModule";
    case VK_OBJECT_TYPE_PIPELINE_CACHE:
        return "PipelineCache";
    case VK_OBJECT_TYPE_PIPELINE_LAYOUT:
        return "PipelineLayout";
    case VK_OBJECT_TYPE_RENDER_PASS:
        return "RenderPass";
    case VK_OBJECT_TYPE_PIPELINE:
        return "Pipeline";
    case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
        return "DescriptorSetLayout";
    case VK_OBJECT_TYPE_SAMPLER:
        return "Sampler";
    case VK_OBJECT_TYPE_DESCRIPTOR_POOL:
        return "DescriptorPool";
    case VK_OBJECT_TYPE_DESCRIPTOR_SET:
        return "DescriptorSet";
    case VK_OBJECT_TYPE_FRAMEBUFFER:
        return "Framebuffer";
    case VK_OBJECT_TYPE_COMMAND_POOL:
        return "CommandPool";
    case VK_OBJECT_TYPE_SWAPCHAIN_KHR:
        return "SwapchainKHR";
    default:
        return "Object";
    }
}

const char*
type_str(VkDebugUtilsMessageTypeFlagsEXT t)
{
    // Highest-signal bit wins when multiple are set.
    if (t & VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT)
    {
        return "BDA";
    }
    if (t & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
    {
        return "PERF";
    }
    if (t & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
    {
        return "VALID";
    }
    return "GEN";
}

void
format_context(std::string& out, const VkDebugUtilsMessengerCallbackDataEXT* d)
{
    if (d->queueLabelCount > 0)
    {
        out += "\n  queue: ";
        for (uint32_t i = 0; i < d->queueLabelCount; ++i)
        {
            if (i)
            {
                out += " > ";
            }
            out += d->pQueueLabels[i].pLabelName ? d->pQueueLabels[i].pLabelName : "?";
        }
    }
    if (d->cmdBufLabelCount > 0)
    {
        out += "\n  cmd: ";
        for (uint32_t i = 0; i < d->cmdBufLabelCount; ++i)
        {
            if (i)
            {
                out += " > ";
            }
            out += d->pCmdBufLabels[i].pLabelName ? d->pCmdBufLabels[i].pLabelName : "?";
        }
    }
    if (d->objectCount > 0)
    {
        out += "\n  objects:";
        for (uint32_t i = 0; i < d->objectCount; ++i)
        {
            const auto& obj = d->pObjects[i];
            out += "\n    [";
            out += object_type_str(obj.objectType);
            out += "] ";
            if (obj.pObjectName)
            {
                out += "\"";
                out += obj.pObjectName;
                out += "\" ";
            }
            char buf[32];
            std::snprintf(
                buf, sizeof(buf), "0x%llx", static_cast<unsigned long long>(obj.objectHandle));
            out += buf;
        }
    }
}

bool
debugger_attached()
{
#if defined(_WIN32)
    return ::IsDebuggerPresent() != 0;
#else
    return false;
#endif
}

void
break_into_debugger()
{
#if defined(_WIN32)
    if (debugger_attached())
    {
        __debugbreak();
    }
#endif
}

PFN_vkSetDebugUtilsObjectNameEXT s_set_object_name = nullptr;
PFN_vkCmdBeginDebugUtilsLabelEXT s_cmd_begin = nullptr;
PFN_vkCmdEndDebugUtilsLabelEXT s_cmd_end = nullptr;
PFN_vkCmdInsertDebugUtilsLabelEXT s_cmd_insert = nullptr;
PFN_vkQueueBeginDebugUtilsLabelEXT s_queue_begin = nullptr;
PFN_vkQueueEndDebugUtilsLabelEXT s_queue_end = nullptr;

}  // namespace

void
debug_reset_dedupe()
{
    auto& d = dedupe();
    std::lock_guard<std::mutex> lk(d.mtx);
    d.seen.clear();
}

void
debug_init(VkInstance instance)
{
    if (instance == VK_NULL_HANDLE)
    {
        return;
    }

    s_set_object_name = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
        vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT"));
    s_cmd_begin = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT"));
    s_cmd_end = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT"));
    s_cmd_insert = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance, "vkCmdInsertDebugUtilsLabelEXT"));
    s_queue_begin = reinterpret_cast<PFN_vkQueueBeginDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance, "vkQueueBeginDebugUtilsLabelEXT"));
    s_queue_end = reinterpret_cast<PFN_vkQueueEndDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance, "vkQueueEndDebugUtilsLabelEXT"));
}

VKAPI_ATTR VkBool32 VKAPI_CALL
debug_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                         VkDebugUtilsMessageTypeFlagsEXT type,
                         const VkDebugUtilsMessengerCallbackDataEXT* data,
                         void* /*user_data*/)
{
    if (!data)
    {
        return VK_FALSE;
    }

    const bool is_error = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0;
    const bool is_warn = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0;
    const bool is_info = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0;

    // Dedupe non-error messages. Errors always log.
    if (!is_error && already_seen(data->messageIdNumber, data->pMessage))
    {
        return VK_FALSE;
    }

    std::string body;
    body.reserve(512);
    body += "[VK/";
    body += type_str(type);
    body += "] ";
    if (data->pMessageIdName)
    {
        body += data->pMessageIdName;
        body += " ";
    }
    if (data->pMessage)
    {
        body += data->pMessage;
    }
    format_context(body, data);

    if (is_error)
    {
        ALOG_ERROR("{}", body);
        break_into_debugger();
    }
    else if (is_warn)
    {
        ALOG_WARN("{}", body);
    }
    else if (is_info)
    {
        ALOG_INFO("{}", body);
    }
    else
    {
        ALOG_TRACE("{}", body);
    }

    return VK_FALSE;  // never abort the call
}

// --- Object naming --------------------------------------------------------

void
set_debug_object_name_raw(VkDevice device,
                          VkObjectType type,
                          uint64_t handle,
                          std::string_view name)
{
    if (device == VK_NULL_HANDLE || handle == 0 || name.empty())
    {
        return;
    }

    // std::string ensures null-termination for the C API.
    std::string nul_terminated(name);

    VkDebugUtilsObjectNameInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType = type;
    info.objectHandle = handle;
    info.pObjectName = nul_terminated.c_str();

    if (s_set_object_name)
    {
        s_set_object_name(device, &info);
    }
}

// --- Labels ---------------------------------------------------------------

namespace
{

VkDebugUtilsLabelEXT
make_label(const std::string& nul_name, float r, float g, float b, float a)
{
    VkDebugUtilsLabelEXT l{};
    l.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    l.pLabelName = nul_name.c_str();
    l.color[0] = r;
    l.color[1] = g;
    l.color[2] = b;
    l.color[3] = a;
    return l;
}

}  // namespace

void
cmd_begin_label(VkCommandBuffer cmd, std::string_view name, float r, float g, float b, float a)
{
    if (cmd == VK_NULL_HANDLE || name.empty() || !s_cmd_begin)
    {
        return;
    }
    std::string nn(name);
    auto l = make_label(nn, r, g, b, a);
    s_cmd_begin(cmd, &l);
}

void
cmd_end_label(VkCommandBuffer cmd)
{
    if (cmd == VK_NULL_HANDLE || !s_cmd_end)
    {
        return;
    }
    s_cmd_end(cmd);
}

void
cmd_insert_label(VkCommandBuffer cmd, std::string_view name, float r, float g, float b, float a)
{
    if (cmd == VK_NULL_HANDLE || name.empty() || !s_cmd_insert)
    {
        return;
    }
    std::string nn(name);
    auto l = make_label(nn, r, g, b, a);
    s_cmd_insert(cmd, &l);
}

scoped_cmd_label::scoped_cmd_label(
    VkCommandBuffer cmd, std::string_view name, float r, float g, float b, float a)
    : m_cmd(cmd)
{
    cmd_begin_label(m_cmd, name, r, g, b, a);
}

scoped_cmd_label::~scoped_cmd_label()
{
    cmd_end_label(m_cmd);
}

void
queue_begin_label(VkQueue queue, std::string_view name, float r, float g, float b, float a)
{
    if (queue == VK_NULL_HANDLE || name.empty() || !s_queue_begin)
    {
        return;
    }
    std::string nn(name);
    auto l = make_label(nn, r, g, b, a);
    s_queue_begin(queue, &l);
}

void
queue_end_label(VkQueue queue)
{
    if (queue == VK_NULL_HANDLE || !s_queue_end)
    {
        return;
    }
    s_queue_end(queue);
}

}  // namespace vk_utils
}  // namespace render
}  // namespace kryga
