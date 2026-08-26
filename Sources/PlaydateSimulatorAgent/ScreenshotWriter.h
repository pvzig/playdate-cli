#ifndef SCREENSHOT_WRITER_H
#define SCREENSHOT_WRITER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool pdsim_write_screenshot_png(
    const uint8_t *framebuffer,
    const char *path,
    char *error_message,
    size_t error_message_capacity
);

#endif
