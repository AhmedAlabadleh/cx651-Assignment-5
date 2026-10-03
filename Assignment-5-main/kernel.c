#include "loader.h"
#include <stdlib.h>
#include <string.h>

/** Returns p1 with each channel multiplied by scalar. */
struct pixel mul(struct pixel p1, float scalar) {
    return (struct pixel){.r = (int)(p1.r * scalar),
                          .g = (int)(p1.g * scalar),
                          .b = (int)(p1.b * scalar)};
}
/** Returns the channel-wise sum of p1 and p2. */
struct pixel add(struct pixel p1, struct pixel p2) {
    return (struct pixel){.r = p1.r + p2.r,
                          .g = p1.g + p2.g,
                          .b = p1.b + p2.b};
}

struct image* apply_kernel(struct image* img, int* kernel, int ksize, float normalize) {
    if (!img || !kernel || ksize <= 0 || (ksize % 2) == 0) return NULL;

    int W = img->width;
    int H = img->height;
    int half = ksize / 2;

    struct image* out = malloc(sizeof(struct image));
    out->width  = W;
    out->height = H;
    out->pixels = malloc(sizeof(struct pixel) * (size_t)W * (size_t)H);

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int acc_r = 0, acc_g = 0, acc_b = 0;

            for (int ky = 0; ky < ksize; ky++) {
                for (int kx = 0; kx < ksize; kx++) {
                    int iy = y + ky - half;
                    int ix = x + kx - half;

                    int pr = 0, pg = 0, pb = 0;
                    if (iy >= 0 && iy < H && ix >= 0 && ix < W) {
                        struct pixel p = img->pixels[iy * W + ix];
                        pr = p.r; pg = p.g; pb = p.b;
                    }
                    int kv = kernel[ky * ksize + kx];
                    acc_r += kv * pr;
                    acc_g += kv * pg;
                    acc_b += kv * pb;
                }
            }

            /* apply normalization and clamp to [0,255] */
            int r = (int)(acc_r * normalize);
            int g = (int)(acc_g * normalize);
            int b = (int)(acc_b * normalize);
            if (r < 0) r = 0; if (r > 255) r = 255;
            if (g < 0) g = 0; if (g > 255) g = 255;
            if (b < 0) b = 0; if (b > 255) b = 255;

            out->pixels[y * W + x] = (struct pixel){.r = r, .g = g, .b = b};
        }
    }
    return out;
}