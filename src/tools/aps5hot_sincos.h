// SPDX-License-Identifier: MIT
// The game's libc sincos (FreeBSD msun s_sincos / k_sin / k_cos / e_rem_pio2, built by the game's
// toolchain with FMA contraction) re-expressed operation by operation, so native results are
// bit-identical to the x86 code. Huge arguments (|x| >= 2^20*pi/2) use the host libm instead
// (at most 1 ulp apart; no game angle is that large). FP contraction is off: every fused
// multiply-add is the explicit fma() the game's code has.
#pragma STDC FP_CONTRACT OFF
#include <math.h>
#include <stdint.h>
#include <string.h>

static inline double GameKernelSin(double x, double y, int iy) {
    const double S1 = -0x1.5555555555549p-3, S2 = 0x1.111111110f8a6p-7, S3 = -0x1.a01a019c161d5p-13,
                 S4 = 0x1.71de357b1fe7dp-19, S5 = -0x1.ae5e68a2b9cebp-26, S6 = 0x1.5d93a5acfd57cp-33;
    const double z = x * x;
    const double a = fma(z, fma(z, S4, S3), S2);
    const double w = z * z;
    const double r = fma(z * w, fma(z, S6, S5), a);
    const double v = z * x;
    if (iy == 0) return fma(v, fma(z, r, S1), x);
    double t = fma(y, 0.5, -(v * r));
    t = fma(z, t, -y);
    t = fma(v, -S1, t);
    return x - t;
}

static inline double GameKernelCos(double x, double y) {
    const double C1 = 0x1.555555555554cp-5, C2 = -0x1.6c16c16c15177p-10, C3 = 0x1.a01a019cb1590p-16,
                 C4 = -0x1.27e4f809c52adp-22, C5 = 0x1.1ee9ebdb4b1c4p-29, C6 = -0x1.8fae9be8838d4p-37;
    const double z = x * x;
    const double p = fma(z, fma(z, C6, C5), C4);
    const double q = fma(z, fma(z, C3, C2), C1);
    const double xy = x * y;
    const double w = z * z;
    const double r = fma(z, q, (w * w) * p);
    const double hz = z * 0.5;
    const double t = fma(z, r, -xy);
    const double w1 = 1.0 - hz;
    return w1 + (((1.0 - w1) - hz) + t);
}

static inline void GameSinCos(double x, double* sn, double* cs) {
    const double pio2_1 = 0x1.921fb54400000p+0, pio2_1t = 0x1.0b4611a626331p-34, pio2_2 = 0x1.0b4611a600000p-34,
                 pio2_2t = 0x1.3198a2e037073p-69, pio2_3 = 0x1.3198a2e000000p-69, pio2_3t = 0x1.b839a252049c1p-104,
                 invpio2 = 0x1.45f306dc9c883p-1;
    uint64_t bits;
    memcpy(&bits, &x, sizeof bits);
    const int32_t hx = (int32_t)(bits >> 32);
    const uint32_t ix = (uint32_t)hx & 0x7fffffff;
    if (ix <= 0x3fe921fb) {
        *sn = ix <= 0x3e4fffff ? x : GameKernelSin(x, 0, 0);
        *cs = ix <= 0x3e46a09d ? 1.0 : GameKernelCos(x, 0);
        return;
    }
    if (ix >= 0x7ff00000) {
        *sn = *cs = x - x;
        return;
    }
    double y0, y1, z;
    int n;
    // Multiples of pi/2 up to 4: one subtraction of a two-part constant.
    if (ix <= 0x400f6a7a && (ix & 0xfffff) != 0x921fb) {
        const double k = ix <= 0x4002d97c ? 1.0 : 2.0;
        n = ix <= 0x4002d97c ? 1 : 2;
        if (hx > 0) { z = x + -(k * pio2_1); y0 = z + -(k * pio2_1t); y1 = (z - y0) + -(k * pio2_1t); }
        else { z = x + k * pio2_1; y0 = z + k * pio2_1t; y1 = (z - y0) + k * pio2_1t; n = -n; }
    } else if (ix > 0x400f6a7a && ix <= 0x401c463b && ix != 0x4012d97c && ix != 0x401921fb) {
        const int three = ix <= 0x4015fdbc;
        const double kz = three ? 0x1.2d97c7f300000p+2 : 0x1.921fb54400000p+2;
        const double kt = three ? 0x1.90e91a79394cap-33 : 0x1.0b4611a626331p-32;
        n = three ? 3 : 4;
        if (hx > 0) { z = x + -kz; y0 = z + -kt; y1 = (z - y0) + -kt; }
        else { z = x + kz; y0 = z + kt; y1 = (z - y0) + kt; n = -n; }
    } else if (ix <= 0x413921fa) {
        const double fn = fma(x, invpio2, 0x1.8p52) + -0x1.8p52;
        n = (int)fn;
        double r = fma(fn, -pio2_1, x);
        double w = fn * pio2_1t;
        y0 = r - w;
        const uint32_t j = ix >> 20;
        memcpy(&bits, &y0, sizeof bits);
        if ((int32_t)(j - ((bits >> 52) & 0x7ff)) > 16) {
            double t = r;
            w = fn * pio2_2;
            r = t - w;
            w = fma(fn, pio2_2t, -((t - r) - w));
            y0 = r - w;
            memcpy(&bits, &y0, sizeof bits);
            if ((int32_t)(j - ((bits >> 52) & 0x7ff)) > 49) {
                t = r;
                w = fn * pio2_3;
                r = t - w;
                w = fma(fn, pio2_3t, -((t - r) - w));
                y0 = r - w;
            }
        }
        y1 = (r - y0) - w;
    } else {
        sincos(x, sn, cs);
        return;
    }
    switch (n & 3) {
    case 0: *sn = GameKernelSin(y0, y1, 1); *cs = GameKernelCos(y0, y1); break;
    case 1: *sn = GameKernelCos(y0, y1); *cs = -GameKernelSin(y0, y1, 1); break;
    case 2: *sn = -GameKernelSin(y0, y1, 1); *cs = -GameKernelCos(y0, y1); break;
    default: *sn = -GameKernelCos(y0, y1); *cs = GameKernelSin(y0, y1, 1); break;
    }
}
