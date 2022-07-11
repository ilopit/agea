#include "engine/rendering queues.h"

#include "model/rendering/renderable.h"
#include "vulkan_render_types/vulkan_render_data.h"
#include "utils/agea_log.h"

namespace agea
{

void
rendering_queues::add_to_queue(model::renderable* obj)
{
    m_queues[obj->m_render_data->gen_render_data_id()].push_back(obj->m_render_data.get());
}

void
rendering_queues::add_to_dirty_queue(model::renderable* r)
{
    ALOG_INFO("Added {0} to diry queue", r->m_owner_id.cstr());
    m_dirty_object.push_back(r);
}

void
rendering_queues::clear_dirty_queue()
{
    if (!m_dirty_object.empty())
    {
        ALOG_INFO("Clear diry objecs queue");
        m_dirty_object.clear();
    }
}

void
rendering_queues::remove_from_rdc(model::renderable* r)
{
    auto key = r->m_render_data->gen_render_data_id();
    auto obj = r->m_render_data.get();

    auto qitr = m_queues.find(key);
    if (qitr == m_queues.end())
    {
        return;
    }

    auto& queue = qitr->second;

    auto itr = std::find(queue.begin(), queue.end(), obj);

    if (itr != queue.end())
    {
        std::swap(*itr, queue.back());
        queue.pop_back();

        if (queue.empty())
        {
            m_queues.erase(key);
        }
    }
}

}  // namespace agea