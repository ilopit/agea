#pragma once

#include "utils/weird_singletone.h"

namespace agea
{
namespace model
{
class renderable;
}

struct model_render_api
{
    virtual void
    add_to_render_queue(model::renderable*)
    {
    }

    virtual void
    invalidate(model::renderable*)
    {
    }

    virtual void
    remove_from_render_queue(model::renderable*)
    {
    }
};

namespace glob
{
struct model_render_api : public simple_singleton<::agea::model_render_api*>
{
};

}  // namespace glob

}  // namespace agea