// Spherical Harmonics utilities (L2 / order 2)
// 9 basis functions, evaluated for a given direction

#include "gpu_types/gpu_probe_types.h"

#define SH_PI 3.14159265358979323846

// SH L2 basis functions for direction d (must be normalized)
// Returns 9 coefficients: L0 (1), L1 (3), L2 (5)
void sh_eval_basis(vec3 d, out float basis[9])
{
    // L0
    basis[0] = 0.282095;  // 1/(2*sqrt(pi))

    // L1
    basis[1] = 0.488603 * d.y;   // sqrt(3)/(2*sqrt(pi)) * y
    basis[2] = 0.488603 * d.z;   // sqrt(3)/(2*sqrt(pi)) * z
    basis[3] = 0.488603 * d.x;   // sqrt(3)/(2*sqrt(pi)) * x

    // L2
    basis[4] = 1.092548 * d.x * d.y;                    // sqrt(15)/(2*sqrt(pi)) * xy
    basis[5] = 1.092548 * d.y * d.z;                    // sqrt(15)/(2*sqrt(pi)) * yz
    basis[6] = 0.315392 * (3.0 * d.z * d.z - 1.0);     // sqrt(5)/(4*sqrt(pi)) * (3z^2-1)
    basis[7] = 1.092548 * d.x * d.z;                    // sqrt(15)/(2*sqrt(pi)) * xz
    basis[8] = 0.546274 * (d.x * d.x - d.y * d.y);     // sqrt(15)/(4*sqrt(pi)) * (x^2-y^2)
}

// Reconstruct irradiance from SH probe for a given surface normal
vec3 sh_evaluate(sh_probe probe, vec3 normal)
{
    float basis[9];
    sh_eval_basis(normal, basis);

    vec3 result = vec3(0.0);

    // Unpack coefficients from the 7 vec4s
    // Each channel (R,G,B) has 9 coefficients packed sequentially
    float coeffs[28];
    for (int i = 0; i < 7; i++)
    {
        coeffs[i * 4 + 0] = probe.coefficients[i].x;
        coeffs[i * 4 + 1] = probe.coefficients[i].y;
        coeffs[i * 4 + 2] = probe.coefficients[i].z;
        coeffs[i * 4 + 3] = probe.coefficients[i].w;
    }

    // R channel: coefficients 0-8
    for (int i = 0; i < 9; i++)
        result.r += coeffs[i] * basis[i];

    // G channel: coefficients 9-17
    for (int i = 0; i < 9; i++)
        result.g += coeffs[9 + i] * basis[i];

    // B channel: coefficients 18-26
    for (int i = 0; i < 9; i++)
        result.b += coeffs[18 + i] * basis[i];

    return max(result, vec3(0.0));
}
