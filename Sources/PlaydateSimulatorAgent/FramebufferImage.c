#include "FramebufferImage.h"

#include <stdbool.h>
#include <stdlib.h>

static void pdsim_release_pixels(void *info, const void *data, size_t size) {
    (void)info;
    (void)size;
    free((void *)data);
}

CGImageRef pdsim_create_framebuffer_image(const uint8_t *framebuffer) {
    if (framebuffer == NULL) {
        return NULL;
    }

    size_t pixel_count = PDSIM_SCREEN_WIDTH * PDSIM_SCREEN_HEIGHT;
    uint8_t *pixels = malloc(pixel_count);
    if (pixels == NULL) {
        return NULL;
    }

    for (size_t y = 0; y < PDSIM_SCREEN_HEIGHT; y++) {
        for (size_t x = 0; x < PDSIM_SCREEN_WIDTH; x++) {
            uint8_t byte = framebuffer[y * PDSIM_FRAMEBUFFER_BYTES_PER_ROW + x / 8];
            bool is_white = (byte & (uint8_t)(0x80U >> (x % 8))) != 0;
            pixels[y * PDSIM_SCREEN_WIDTH + x] = is_white ? UINT8_MAX : 0;
        }
    }

    CGDataProviderRef provider =
        CGDataProviderCreateWithData(NULL, pixels, pixel_count, pdsim_release_pixels);
    if (provider == NULL) {
        free(pixels);
        return NULL;
    }

    CGColorSpaceRef color_space = CGColorSpaceCreateDeviceGray();
    if (color_space == NULL) {
        CGDataProviderRelease(provider);
        return NULL;
    }

    CGImageRef image = CGImageCreate(
        PDSIM_SCREEN_WIDTH,
        PDSIM_SCREEN_HEIGHT,
        8,
        8,
        PDSIM_SCREEN_WIDTH,
        color_space,
        (CGBitmapInfo)kCGImageAlphaNone,
        provider,
        NULL,
        false,
        kCGRenderingIntentDefault
    );
    CGColorSpaceRelease(color_space);
    CGDataProviderRelease(provider);
    return image;
}
