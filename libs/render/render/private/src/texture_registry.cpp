#include "vulkan_render/texture_registry.h"

#include "vulkan_render/types/vulkan_texture_data.h"

#include <utils/kryga_log.h>

namespace kryga
{

glob::texture_registry::type glob::texture_registry::type::s_instance;

namespace render
{

uint32_t
texture_registry::register_texture(texture_data* tex)
{
    if (!tex)
    {
        return INVALID_INDEX;
    }

    uint32_t index;

    if (!m_free_list.empty())
    {
        index = m_free_list.back();
        m_free_list.pop_back();
        m_textures[index] = tex;
    }
    else
    {
        if (m_textures.size() >= MAX_TEXTURES)
        {
            ALOG_ERROR("Texture registry full! Max {} textures reached.", MAX_TEXTURES);
            return INVALID_INDEX;
        }
        index = static_cast<uint32_t>(m_textures.size());
        m_textures.push_back(tex);
    }

    m_dirty.push_back(index);
    ++m_count;

    return index;
}

void
texture_registry::unregister_texture(uint32_t index)
{
    if (index >= m_textures.size() || index == INVALID_INDEX)
    {
        return;
    }

    if (m_textures[index] != nullptr)
    {
        m_textures[index] = nullptr;
        m_free_list.push_back(index);
        --m_count;
    }
}

texture_data*
texture_registry::get(uint32_t index) const
{
    if (index >= m_textures.size() || index == INVALID_INDEX)
    {
        return nullptr;
    }
    return m_textures[index];
}

}  // namespace render
}  // namespace kryga
