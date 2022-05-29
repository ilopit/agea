#pragma once

#include "core/agea_minimal.h"

namespace agea
{
namespace render
{
struct render_data;
}
namespace model
{
class renderable
{
public:
    renderable();
    ~renderable();

    virtual void
    register_for_rendering() = 0;

    virtual bool
    prepare_for_rendering() = 0;

    bool
    mark_dirty();

    std::unique_ptr<render::render_data> m_render_data;
    bool m_dirty = true;

    std::string m_owner_id;
};
}  // namespace model
}  // namespace agea