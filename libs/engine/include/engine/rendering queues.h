#pragma once

#include <unordered_map>
#include <string>
#include <vector>

namespace agea
{

namespace render
{
struct render_data;
}
namespace model
{
class renderable;
}  // namespace model

class rendering_queues
{
public:
    void
    add_to_queue(model::renderable* obj);

    void
    add_to_dirty_queue(model::renderable* r);

    void
    clear_dirty_queue();

    void
    remove_from_rdc(model::renderable* obj);

    std::vector<model::renderable*>&
    dirty_objects()
    {
        return m_dirty_object;
    }

    std::unordered_map<std::string, std::vector<render::render_data*>>&
    queues()
    {
        return m_queues;
    }

private:
    std::unordered_map<std::string, std::vector<render::render_data*>> m_queues;
    std::vector<model::renderable*> m_dirty_object;
};

}  // namespace agea