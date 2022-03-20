#pragma once

#include <vector>
#include <functional>
#include <string>

#include <vulkan/vulkan.h>

#include "utils/weird_singletone.h"
#include "reflection/types.h"

namespace agea
{
namespace model
{
class level_object_component;
class level_object;
}  // namespace model

namespace ui
{
class window
{
public:
    window(std::string name)
        : m_window(true)
        , m_str(std::move(name))
    {
    }

    virtual void handle();

    bool m_window;
    std::string m_str;
};

class editor_window : public window
{
public:
    editor_window(std::string name)
        : window(std::move(name))
    {
    }

    void handle() override;

    void draw_components(model::level_object_component* obj);
    void draw_oject(model::level_object* obj);
    void draw_oject_editor(model::level_object* obj);
};

class ui
{
public:
    ui();

    void new_frame();

    void draw(VkCommandBuffer cmd);

    std::vector<std::unique_ptr<window>> m_winodws;
};

class property_drawers
{
public:
    static void init();

    static void draw_t_str(char* v);

    static void draw_t_i8(char* v);
    static void draw_t_i16(char* v);
    static void draw_t_i32(char* v);
    static void draw_t_i64(char* v);

    static void draw_t_u8(char* v);
    static void draw_t_u16(char* v);
    static void draw_t_u32(char* v);
    static void draw_t_u64(char* v);

    static void draw_t_f(char* v);
    static void draw_t_d(char* v);

    static std::function<void(char*)> drawers[(size_t)reflection::supported_type::t_last];
};

}  // namespace ui

namespace glob
{
struct ui : public weird_singleton<::agea::ui::ui>
{
};
}  // namespace glob
}  // namespace agea