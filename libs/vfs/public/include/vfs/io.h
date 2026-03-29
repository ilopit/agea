#pragma once

#include <vfs/rid.h>
#include <utils/buffer.h>

namespace kryga
{
namespace vfs
{

bool
load_buffer(const rid& id, utils::buffer& b);

bool
save_buffer(utils::buffer& b);

bool
load_file(const rid& id, std::vector<uint8_t>& blob);

bool
save_file(const rid& id, const std::vector<uint8_t>& blob);

}  // namespace vfs
}  // namespace kryga
