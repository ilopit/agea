#include "utils/buffer.h"

#include "utils/agea_log.h"

namespace agea
{
namespace utils
{

bool
buffer::load(const utils::path& p, buffer& b)
{
    if (!file_utils::load_file(p, b.m_data))
    {
        return false;
    }
    b.m_file = p;
    b.m_last_write_time = std::filesystem::last_write_time(b.m_file.fs());

    return true;
}

bool
buffer::has_file_updated() const
{
    auto current_time = std::filesystem::last_write_time(m_file.fs());
    auto updated = m_last_write_time < current_time;
    if (updated)
    {
        ALOG_INFO("{0} was updated", m_file.str());
    }
    return updated;
}

bool
buffer::consume_file_updated()
{
    auto current_time = std::filesystem::last_write_time(m_file.fs());
    auto updated = m_last_write_time < current_time;
    if (updated)
    {
        ALOG_INFO("{0} was updated", m_file.str());
        m_last_write_time = current_time;
    }
    return updated;
}
}  // namespace utils
}  // namespace agea