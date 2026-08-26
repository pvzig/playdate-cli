#ifndef GIF_RECORDER_H
#define GIF_RECORDER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef bool (*pdsim_capture_frame_function)(uint8_t *framebuffer, void *context);

// Start and stop must be called from the agent's serial execution context.
// Stop synchronously publishes the finished no-overwrite output file.
bool pdsim_gif_recorder_start(
    const char *path,
    pdsim_capture_frame_function capture_frame,
    void *capture_context,
    char *error_message,
    size_t error_message_capacity
);

bool pdsim_gif_recorder_stop(
    char *error_message,
    size_t error_message_capacity
);

#endif
