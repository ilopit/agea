#include <animation/ozz_loader.h>

#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>

#include <utils/kryga_log.h>

namespace kryga
{
namespace animation
{

bool
ozz_loader::load_skeleton(const std::string& path, ozz::animation::Skeleton& out_skeleton)
{
    ozz::io::File file(path.c_str(), "rb");
    if (!file.opened())
    {
        ALOG_ERROR("ozz_loader: Failed to open skeleton file '{}'", path);
        return false;
    }

    ozz::io::IArchive archive(&file);
    if (!archive.TestTag<ozz::animation::Skeleton>())
    {
        ALOG_ERROR("ozz_loader: File '{}' does not contain a valid skeleton", path);
        return false;
    }

    archive >> out_skeleton;

    ALOG_INFO("ozz_loader: Loaded skeleton from '{}' ({} joints)", path, out_skeleton.num_joints());
    return true;
}

bool
ozz_loader::load_animation(const std::string& path, ozz::animation::Animation& out_animation)
{
    ozz::io::File file(path.c_str(), "rb");
    if (!file.opened())
    {
        ALOG_ERROR("ozz_loader: Failed to open animation file '{}'", path);
        return false;
    }

    ozz::io::IArchive archive(&file);
    if (!archive.TestTag<ozz::animation::Animation>())
    {
        ALOG_ERROR("ozz_loader: File '{}' does not contain a valid animation", path);
        return false;
    }

    archive >> out_animation;

    ALOG_INFO("ozz_loader: Loaded animation from '{}' (duration: {:.2f}s, {} tracks)",
              path,
              out_animation.duration(),
              out_animation.num_tracks());
    return true;
}

}  // namespace animation
}  // namespace kryga
