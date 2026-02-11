#pragma once

#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/animation.h>

#include <string>

namespace kryga
{
namespace animation
{

class ozz_loader
{
public:
    static bool
    load_skeleton(const std::string& path, ozz::animation::Skeleton& out_skeleton);

    static bool
    load_animation(const std::string& path, ozz::animation::Animation& out_animation);
};

}  // namespace animation
}  // namespace kryga
