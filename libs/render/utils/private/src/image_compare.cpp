#include <render/utils/image_compare.h>

#include <stb_unofficial/stb.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace kryga::render
{

namespace
{

float
to_luminance(uint8_t r, uint8_t g, uint8_t b)
{
    return 0.299f * r + 0.587f * g + 0.114f * b;
}

// SSIM constants for 8-bit images
constexpr float C1 = (0.01f * 255.0f) * (0.01f * 255.0f);
constexpr float C2 = (0.03f * 255.0f) * (0.03f * 255.0f);

constexpr uint32_t SSIM_WINDOW = 8;

}  // namespace

float
compute_ssim(const uint8_t* img_a, const uint8_t* img_b, uint32_t width, uint32_t height)
{
    if (width < SSIM_WINDOW || height < SSIM_WINDOW)
    {
        return 1.0f;
    }

    uint32_t windows_x = width / SSIM_WINDOW;
    uint32_t windows_y = height / SSIM_WINDOW;
    double total_ssim = 0.0;
    uint32_t window_count = 0;

    for (uint32_t wy = 0; wy < windows_y; ++wy)
    {
        for (uint32_t wx = 0; wx < windows_x; ++wx)
        {
            float mean_a = 0.0f;
            float mean_b = 0.0f;

            for (uint32_t py = 0; py < SSIM_WINDOW; ++py)
            {
                for (uint32_t px = 0; px < SSIM_WINDOW; ++px)
                {
                    uint32_t idx = ((wy * SSIM_WINDOW + py) * width + (wx * SSIM_WINDOW + px)) * 4;
                    mean_a += to_luminance(img_a[idx], img_a[idx + 1], img_a[idx + 2]);
                    mean_b += to_luminance(img_b[idx], img_b[idx + 1], img_b[idx + 2]);
                }
            }

            constexpr float n = SSIM_WINDOW * SSIM_WINDOW;
            mean_a /= n;
            mean_b /= n;

            float var_a = 0.0f;
            float var_b = 0.0f;
            float covar = 0.0f;

            for (uint32_t py = 0; py < SSIM_WINDOW; ++py)
            {
                for (uint32_t px = 0; px < SSIM_WINDOW; ++px)
                {
                    uint32_t idx = ((wy * SSIM_WINDOW + py) * width + (wx * SSIM_WINDOW + px)) * 4;
                    float la = to_luminance(img_a[idx], img_a[idx + 1], img_a[idx + 2]) - mean_a;
                    float lb = to_luminance(img_b[idx], img_b[idx + 1], img_b[idx + 2]) - mean_b;
                    var_a += la * la;
                    var_b += lb * lb;
                    covar += la * lb;
                }
            }

            var_a /= (n - 1.0f);
            var_b /= (n - 1.0f);
            covar /= (n - 1.0f);

            float numerator = (2.0f * mean_a * mean_b + C1) * (2.0f * covar + C2);
            float denominator = (mean_a * mean_a + mean_b * mean_b + C1) * (var_a + var_b + C2);

            total_ssim += numerator / denominator;
            ++window_count;
        }
    }

    return window_count > 0 ? static_cast<float>(total_ssim / window_count) : 1.0f;
}

image_compare_result
compare_images(const uint8_t* actual, const uint8_t* expected, const image_compare_params& params)
{
    image_compare_result result;
    result.total_pixels = params.width * params.height;

    if (params.generate_diff_image)
    {
        result.diff_image.resize(result.total_pixels * 4);
    }

    for (uint32_t i = 0; i < result.total_pixels; ++i)
    {
        uint32_t idx = i * 4;
        int dr = std::abs(static_cast<int>(actual[idx]) - static_cast<int>(expected[idx]));
        int dg = std::abs(static_cast<int>(actual[idx + 1]) - static_cast<int>(expected[idx + 1]));
        int db = std::abs(static_cast<int>(actual[idx + 2]) - static_cast<int>(expected[idx + 2]));
        int da = std::abs(static_cast<int>(actual[idx + 3]) - static_cast<int>(expected[idx + 3]));

        bool match = dr <= params.pixel_tolerance && dg <= params.pixel_tolerance &&
                     db <= params.pixel_tolerance && da <= params.pixel_tolerance;

        if (!match)
        {
            ++result.diff_pixel_count;
        }

        if (params.generate_diff_image)
        {
            uint8_t magnitude = static_cast<uint8_t>(std::min(255, (dr + dg + db + da) * 4));
            if (match)
            {
                result.diff_image[idx] = 0;
                result.diff_image[idx + 1] = 128;
                result.diff_image[idx + 2] = 0;
                result.diff_image[idx + 3] = 255;
            }
            else
            {
                result.diff_image[idx] = std::max(magnitude, uint8_t(128));
                result.diff_image[idx + 1] = 0;
                result.diff_image[idx + 2] = 0;
                result.diff_image[idx + 3] = 255;
            }
        }
    }

    result.diff_percentage =
        result.total_pixels > 0
            ? (static_cast<float>(result.diff_pixel_count) / result.total_pixels) * 100.0f
            : 0.0f;

    result.ssim = compute_ssim(actual, expected, params.width, params.height);

    result.pixel_passed = (result.diff_pixel_count == 0);
    result.ssim_passed = (result.ssim >= params.ssim_threshold);

    return result;
}

std::vector<uint8_t>
load_png(const std::string& path, uint32_t& width, uint32_t& height)
{
    int w = 0, h = 0, channels = 0;
    uint8_t* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!data)
    {
        return {};
    }

    width = static_cast<uint32_t>(w);
    height = static_cast<uint32_t>(h);

    std::vector<uint8_t> result(data, data + w * h * 4);
    stbi_image_free(data);
    return result;
}

bool
save_png(const std::string& path, const uint8_t* data, uint32_t width, uint32_t height)
{
    return stbi_write_png(path.c_str(), width, height, 4, data, width * 4) != 0;
}

}  // namespace kryga::render
