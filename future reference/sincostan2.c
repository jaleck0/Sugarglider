#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define TABLE_SIZE 64

// Precomputed Q8.8 sine values for θ in [0, 90°] (64 steps)
// Each value = round(sin(θ) * 256)
const int16_t sine_table[TABLE_SIZE] = {
    0, 6, 13, 19, 25, 31, 38, 44,
    50, 56, 62, 69, 75, 81, 87, 93,
    99, 105, 111, 117, 122, 128, 134, 140,
    145, 151, 157, 162, 168, 173, 179, 184,
    189, 195, 200, 205, 210, 216, 221, 226,
    231, 236, 240, 245, 250, 255, 260, 264,
    269, 273, 278, 282, 286, 290, 294, 298,
    302, 306, 310, 313, 317, 320, 324, 327
};

// Returns sin(angle) in Q8.8 format
// angle: uint8_t from 0 to 255 (0° to 360°)
int16_t fixed_sin_q8_8(uint8_t angle) {
    uint8_t quadrant = angle >> 6;     // angle / 64
    uint8_t index = angle & 0x3F;      // angle % 64

    switch (quadrant) {
        case 0:  return  sine_table[index];                   // 0°–89°
        case 1:  return  sine_table[TABLE_SIZE - 1 - index];  // 90°–179°
        case 2:  return -sine_table[index];                   // 180°–269°
        case 3:  return -sine_table[TABLE_SIZE - 1 - index];  // 270°–359°
        default: return 0;
    }
}

// Returns cos(angle) in Q8.8 format
// Equivalent to sin(angle + 64)
int16_t fixed_cos_q8_8(uint8_t angle) {
    return fixed_sin_q8_8(angle + 64);  // uint8_t overflow wraps correctly
}

// Approximate atan(y/x) in uint8_t angle (0–255)
uint8_t fixed_atan2_q8_8(int16_t y, int16_t x) {
    if (x == 0 && y == 0) return 0;

    // Work with absolute values
    int16_t abs_y = abs(y);
    int16_t abs_x = abs(x);

    // z = abs_y / abs_x or abs_x / abs_y depending on which is greater
    uint8_t angle = 0;
    int invert = 0;
    int16_t z_q8_8;

    if (abs_y > abs_x) {
        z_q8_8 = (abs_x << 8) / abs_y;
        invert = 1;
    } else {
        z_q8_8 = (abs_y << 8) / abs_x;
        invert = 0;
    }

    // Apply polynomial approximation:
    // angle ≈ (128/π) * (z - z * |z - 256| * (0.2447 + 0.0663 * |z - 256| / 256))
    // But we'll use a simpler empirical approximation:
    // angle ≈ z * (π/4) ≈ z * 64 / 256 = z / 4 (in uint8_t angle space)

    uint8_t base_angle = z_q8_8 >> 2; // z / 4

    if (invert)
        base_angle = 64 - base_angle; // π/2 - angle

    // Determine final angle based on quadrant
    if (x >= 0 && y >= 0)       return base_angle;          // Q1
    else if (x < 0 && y >= 0)   return 128 - base_angle;    // Q2
    else if (x < 0 && y < 0)    return 128 + base_angle;    // Q3
    else                        return 256 - base_angle;    // Q4
}

// Integer square root of 16-bit number using binary search
uint16_t isqrt16(uint16_t x) {
    uint16_t res = 0;
    uint16_t bit = 1 << 14;  // Start at highest bit for 16-bit sqrt

    while (bit > 0) {
        uint16_t temp = res | bit;
        if (temp * temp <= x) {
            res = temp;
        }
        bit >>= 1;
    }

    return res;
}

// Pythagorean theorem using only 16-bit ints in Q8.8
int16_t pythagoras_q88_16bit(int16_t a_q88, int16_t b_q88) {
    // Convert to absolute value to avoid overflow from squaring negatives
    uint16_t a = (a_q88 < 0) ? -a_q88 : a_q88;
    uint16_t b = (b_q88 < 0) ? -b_q88 : b_q88;

    // Square both: (a^2 >> 8) gives Q16.0 (integer)
    uint16_t a2 = ((uint32_t)a * a) >> 8;
    uint16_t b2 = ((uint32_t)b * b) >> 8;

    // Add squares
    uint16_t sum = a2 + b2;  // Still Q16.0 (unsigned)

    // sqrt(sum) returns Q8.0
    uint16_t sqrt_result = isqrt16(sum);

    // Convert Q8.0 to Q8.8 by shifting left 8
    return (int16_t)(sqrt_result << 4);  // Shift 4 instead of 8 for better resolution
}

int main() {


    printf("\n\n Angle |   sin    |   cos\n");
    printf("-------------------------------\n");
    for (int i = 0; i < 256; i++) {
        uint8_t angle = i;
        int16_t sin_val = fixed_sin_q8_8(angle);
        int16_t cos_val = fixed_cos_q8_8(angle);

        printf("  %3d  | %7.5f | %7.5f\n",
               i,
               sin_val / 256.0,
               cos_val / 256.0);
    }
    
    printf("   y   x  | atan2(y,x) [deg]\n");
    printf("----------------------------\n");

    // Test atan2 on a few known points
    int16_t q8_8_vals[] = {0, 256, -256}; // 0.0, 1.0, -1.0

    for (int i = -10; i <= 10; i++) {
        for (int j = -10; j <= 10; j++) {
            int16_t y = i;
            int16_t x = j;
            uint8_t angle = fixed_atan2_q8_8(y, x);
            double deg = (angle * 360.0) / 256.0;

            printf("%5d %5d | %5d\n", y, x, angle);
        }
    }

    return 0;
}