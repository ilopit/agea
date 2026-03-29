#include "vfs/io.h"

#include "vfs/vfs.h"

#include <global_state/global_state.h>
#include <utils/kryga_log.h>

namespace kryga
{
namespace vfs
{

bool
load_buffer(const rid& id, utils::buffer& b)
{
    auto& vfs = glob::glob_state().getr_vfs();

    if (!vfs.read_bytes(id, b.full_data()))
    {
        return false;
    }

    b.set_vpath(id.str());

    auto rp = vfs.real_path(id);
    if (rp.has_value())
    {
        b.set_file(APATH(rp.value()));
    }

    return true;
}

bool
save_buffer(utils::buffer& b)
{
    auto& vfs = glob::glob_state().getr_vfs();

    auto& vpath = b.get_vpath();
    if (vpath.empty())
    {
        ALOG_ERROR("vfs::save_buffer: buffer has no vpath set");
        return false;
    }

    auto data = std::span<const uint8_t>(b.data(), b.size());
    return vfs.write_bytes(rid(vpath), data);
}

bool
load_file(const rid& id, std::vector<uint8_t>& blob)
{
    auto& vfs = glob::glob_state().getr_vfs();
    return vfs.read_bytes(id, blob);
}

bool
save_file(const rid& id, const std::vector<uint8_t>& blob)
{
    auto& vfs = glob::glob_state().getr_vfs();
    auto data = std::span<const uint8_t>(blob.data(), blob.size());
    return vfs.write_bytes(id, data);
}

}  // namespace vfs
}  // namespace kryga
