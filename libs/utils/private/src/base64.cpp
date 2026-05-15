#include <utils/base64.h>

namespace kryga
{

namespace
{
constexpr char TABLE[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}

std::string
base64_encode(const uint8_t* data, size_t len)
{
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3)
    {
        uint32_t n = uint32_t(data[i]) << 16;
        if (i + 1 < len)
        {
            n |= uint32_t(data[i + 1]) << 8;
        }
        if (i + 2 < len)
        {
            n |= uint32_t(data[i + 2]);
        }
        out.push_back(TABLE[(n >> 18) & 0x3F]);
        out.push_back(TABLE[(n >> 12) & 0x3F]);
        out.push_back(i + 1 < len ? TABLE[(n >> 6) & 0x3F] : '=');
        out.push_back(i + 2 < len ? TABLE[n & 0x3F] : '=');
    }
    return out;
}

std::string
base64_encode(const std::vector<uint8_t>& data)
{
    return base64_encode(data.data(), data.size());
}

}  // namespace kryga
