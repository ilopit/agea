#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace kryga::render
{

struct image_compare_result
{
    bool pixel_passed = false;
    bool ssim_passed = false;

    uint32_t diff_pixel_count = 0;
    uint32_t total_pixels = 0;
    float diff_percentage = 0.0f;
    float ssim = 0.0f;

    // RGBA diff image: red = mismatch, green = match. Empty if not requested.
    std::vector<uint8_t> diff_image;
};

struct image_compare_params
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint8_t pixel_tolerance = 1;
    float ssim_threshold = 0.99f;
    bool generate_diff_image = true;
};

// Compare two RGBA images (width * height * 4 bytes each).
image_compare_result
compare_images(const uint8_t* actual, const uint8_t* expected, const image_compare_params& params);

// Compute SSIM between two RGBA images (converts to luminance internally).
float
compute_ssim(const uint8_t* img_a, const uint8_t* img_b, uint32_t width, uint32_t height);

// Load RGBA PNG. Returns empty vector on failure.
std::vector<uint8_t>
load_png(const std::string& path, uint32_t& width, uint32_t& height);

// Save RGBA data as PNG.
bool
save_png(const std::string& path, const uint8_t* data, uint32_t width, uint32_t height);

}  // namespace kryga::render
