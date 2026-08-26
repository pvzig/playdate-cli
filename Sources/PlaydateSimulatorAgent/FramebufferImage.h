#ifndef FRAMEBUFFER_IMAGE_H
#define FRAMEBUFFER_IMAGE_H

#include "Framebuffer.h"

#include <CoreGraphics/CoreGraphics.h>
#include <stddef.h>
#include <stdint.h>

// The returned image owns a grayscale copy of framebuffer.
CGImageRef pdsim_create_framebuffer_image(const uint8_t *framebuffer);

#endif
