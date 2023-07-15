#pragma once

#include <utils/path.h>
#include <utils/id.h>

#include <utils/singleton_instance.h>

namespace agea
{
namespace editor
{
class config
{
public:
    void
    load(const utils::path& config);

    bool force_recompile_shaders = false;
    uint32_t fps_lock = 30;
    utils::id level;
    uint32_t window_w = 1600;
    uint32_t window_h = 900;
};
}  // namespace editor

namespace glob
{
struct config : public singleton_instance<::agea::editor::config, config>
{
};
}  // namespace glob
}  // namespace agea
