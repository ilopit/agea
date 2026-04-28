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

bool
load_index_manifest(const rid& manifest_id, std::vector<index_entry>& out)
{
    out.clear();

    auto& vfs = glob::glob_state().getr_vfs();
    std::string text;
    if (!vfs.read_string(manifest_id, text))
    {
        return false;
    }

    // One path per line, '#' starts a comment, blank lines skipped.
    size_t pos = 0;
    while (pos < text.size())
    {
        size_t eol = text.find('\n', pos);
        std::string_view line(text.data() + pos,
                              (eol == std::string::npos ? text.size() : eol) - pos);
        pos = (eol == std::string::npos ? text.size() : eol + 1);

        // Trim CR + spaces.
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
        {
            line.remove_suffix(1);
        }
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
        {
            line.remove_prefix(1);
        }
        if (line.empty() || line.front() == '#')
        {
            continue;
        }

        index_entry e;
        e.relative.assign(line);
        auto slash = e.relative.rfind('/');
        auto fname = (slash == std::string::npos) ? e.relative : e.relative.substr(slash + 1);
        auto dot = fname.rfind('.');
        e.stem = (dot == std::string::npos) ? fname : fname.substr(0, dot);
        out.push_back(std::move(e));
    }

    return true;
}

}  // namespace vfs
}  // namespace kryga
