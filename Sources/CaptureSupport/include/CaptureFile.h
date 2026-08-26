#ifndef CAPTURE_FILE_H
#define CAPTURE_FILE_H

#include <stdbool.h>
#include <stddef.h>

// Capture files use short, type-specific recovery paths so the complete path
// always fits in one agent protocol response.
#define PDSIM_CAPTURE_TEMPORARY_PATH_CAPACITY 64

int pdsim_create_screenshot_temporary_file(char *path, size_t capacity);
int pdsim_create_recording_temporary_file(char *path, size_t capacity);

// Publishes without replacing an existing destination. On failure, the
// temporary file remains available at its original recovery path.
bool pdsim_publish_capture_temporary_file(
    const char *temporary_path,
    const char *output_path,
    int *error_number
);

// Fallback for volumes that do not support hard links or exclusive rename.
// Creates the destination with O_EXCL and removes partial output on failure.
bool pdsim_publish_capture_by_exclusive_copy(
    const char *temporary_path,
    const char *output_path,
    int *error_number
);

#endif
