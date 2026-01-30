#pragma once

#include <utils/singleton_instance.h>

#include <cstdint>
#include <vector>

namespace kryga
{
namespace render
{

class texture_data;

class texture_registry
{
public:
    static constexpr uint32_t MAX_TEXTURES = 4096;
    static constexpr uint32_t INVALID_INDEX = UINT32_MAX;

    texture_registry() = default;
    ~texture_registry() = default;

    texture_registry(const texture_registry&) = delete;
    texture_registry& operator=(const texture_registry&) = delete;

    // Register a texture and return its bindless index
    uint32_t
    register_texture(texture_data* tex);

    // Unregister a texture, freeing its index for reuse
    void
    unregister_texture(uint32_t index);

    // Get texture by index (may return nullptr for freed slots)
    texture_data*
    get(uint32_t index) const;

    // Get list of dirty texture indices that need descriptor updates
    const std::vector<uint32_t>&
    get_dirty() const
    {
        return m_dirty;
    }

    // Clear the dirty list after updating descriptors
    void
    clear_dirty()
    {
        m_dirty.clear();
    }

    // Get current count of registered textures
    uint32_t
    get_count() const
    {
        return m_count;
    }

private:
    std::vector<texture_data*> m_textures;
    std::vector<uint32_t> m_free_list;
    std::vector<uint32_t> m_dirty;
    uint32_t m_count = 0;
};

}  // namespace render

namespace glob
{
struct texture_registry : public singleton_instance<::kryga::render::texture_registry, texture_registry>
{
};
}  // namespace glob

}  // namespace kryga
