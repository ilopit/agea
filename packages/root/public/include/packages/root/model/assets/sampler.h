#pragma once

#include "packages/root/model/sampler.ar.h"

#include "packages/root/model/assets/asset.h"

#include <cstdint>

namespace kryga
{
namespace render
{
class sampler_data;
}  // namespace render

namespace root
{

// Sampler filter mode
enum class sampler_filter : uint8_t
{
    nearest = 0,
    linear = 1
};

// Sampler address/wrap mode
enum class sampler_address : uint8_t
{
    repeat = 0,
    mirrored_repeat = 1,
    clamp_to_edge = 2,
    clamp_to_border = 3
};

// clang-format off
KRG_ar_class("architype=sampler",
              render_constructor = sampler__render_loader,
              render_destructor  = sampler__render_destructor);
class sampler : public asset
// clang-format on
{
    KRG_gen_meta__sampler();

public:
    KRG_gen_class_meta(sampler, asset);
    KRG_gen_construct_params{};

    KRG_gen_meta_api;

    render::sampler_data*
    get_sampler_data() const
    {
        return m_sampler_data;
    }

    void
    set_sampler_data(render::sampler_data* v)
    {
        m_sampler_data = v;
    }

protected:
    KRG_ar_property("category=Filter", "access=all", "serializable=true");
    sampler_filter m_min_filter = sampler_filter::linear;

    KRG_ar_property("category=Filter", "access=all", "serializable=true");
    sampler_filter m_mag_filter = sampler_filter::linear;

    KRG_ar_property("category=Address", "access=all", "serializable=true");
    sampler_address m_address_u = sampler_address::repeat;

    KRG_ar_property("category=Address", "access=all", "serializable=true");
    sampler_address m_address_v = sampler_address::repeat;

    KRG_ar_property("category=Quality", "access=all", "serializable=true");
    bool m_anisotropy = false;

    render::sampler_data* m_sampler_data = nullptr;
};

}  // namespace root
}  // namespace kryga
