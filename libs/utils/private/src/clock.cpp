#include "utils/clock.h"

#include <chrono>

namespace kryga
{
namespace utils
{

uint64_t
get_current_time_mks()
{
    using namespace std::chrono;
    return time_point_cast<microseconds>(system_clock::now()).time_since_epoch().count();
}

}  // namespace utils
}  // namespace kryga