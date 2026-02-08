#pragma once

#include "packages/base/model/camera_object.ar.h"

#include "packages/root/model/game_object.h"

namespace kryga
{
namespace base
{

class camera_component;
class input_component;

KRG_ar_class();
class camera_object : public ::kryga::root::game_object
{
public:
    KRG_gen_class_meta(camera_object, game_object);
    KRG_gen_meta_api;

    KRG_gen_construct_params{};

    camera_component*
    get_camera_component()
    {
        return m_camera_component;
    }

    input_component*
    get_camera_input_component()
    {
        return m_camera_input_component;
    }

    void
    begin_play() override;

    void
    end_play() override;

    void
    on_tick(float dt) override;

protected:
    bool
    construct(this_class::construct_params& p);

private:
    void
    on_move_forward(float v);
    void
    on_move_left(float v);
    void
    on_look_up(float v);
    void
    on_look_left(float v);

    camera_component* m_camera_component = nullptr;
    input_component* m_camera_input_component = nullptr;

    float m_forward_delta = 0.f;
    float m_left_delta = 0.f;
    float m_look_up_delta = 0.f;
    float m_look_left_delta = 0.f;

    float m_pitch = 0.f;
    float m_yaw = 0.f;
};

}  // namespace base
}  // namespace kryga
