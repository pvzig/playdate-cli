#include "CaptureFile.h"

#include "DescriptorIO.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PDSIM_SCREENSHOT_TEMPORARY_PATH "/tmp/playdate-simctl-screenshot-XXXXXX.png"
#define PDSIM_RECORDING_TEMPORARY_PATH "/tmp/playdate-simctl-recording-XXXXXX.gif"
#define PDSIM_CAPTURE_EXTENSION_LENGTH 4
#define PDSIM_COPY_BUFFER_CAPACITY 16384

static int pdsim_create_temporary_file(
    const char *path_template,
    char *path,
    size_t capacity
) {
    if (path == NULL || capacity < PDSIM_CAPTURE_TEMPORARY_PATH_CAPACITY) {
        errno = EINVAL;
        return -1;
    }

    int path_length = snprintf(path, capacity, "%s", path_template);
    if (path_length < 0 || (size_t)path_length >= capacity) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return mkstemps(path, PDSIM_CAPTURE_EXTENSION_LENGTH);
}

int pdsim_create_screenshot_temporary_file(char *path, size_t capacity) {
    return pdsim_create_temporary_file(PDSIM_SCREENSHOT_TEMPORARY_PATH, path, capacity);
}

int pdsim_create_recording_temporary_file(char *path, size_t capacity) {
    return pdsim_create_temporary_file(PDSIM_RECORDING_TEMPORARY_PATH, path, capacity);
}

static bool pdsim_copy_file(
    const char *source_path,
    int destination_descriptor,
    int *error_number
) {
    int source_descriptor = open(source_path, O_RDONLY);
    if (source_descriptor < 0) {
        *error_number = errno;
        return false;
    }

    bool copied = true;
    unsigned char buffer[PDSIM_COPY_BUFFER_CAPACITY];
    for (;;) {
        ssize_t length = pdsim_read_retry(source_descriptor, buffer, sizeof(buffer));
        if (length < 0) {
            *error_number = errno;
            copied = false;
            break;
        }
        if (length == 0) {
            break;
        }
        if (pdsim_write_all(destination_descriptor, buffer, (size_t)length) != 0) {
            *error_number = errno;
            copied = false;
            break;
        }
    }

    if (close(source_descriptor) != 0 && copied) {
        *error_number = errno;
        copied = false;
    }
    if (copied && fsync(destination_descriptor) != 0) {
        *error_number = errno;
        copied = false;
    }
    return copied;
}

bool pdsim_publish_capture_by_exclusive_copy(
    const char *temporary_path,
    const char *output_path,
    int *error_number
) {
    int output_descriptor = open(
        output_path,
        O_WRONLY | O_CREAT | O_EXCL,
        0600
    );
    if (output_descriptor < 0) {
        *error_number = errno;
        return false;
    }

    bool copied = pdsim_copy_file(
        temporary_path,
        output_descriptor,
        error_number
    );
    if (close(output_descriptor) != 0 && copied) {
        *error_number = errno;
        copied = false;
    }
    if (!copied) {
        (void)unlink(output_path);
        return false;
    }
    (void)unlink(temporary_path);
    return true;
}

static bool pdsim_create_destination_staging_file(
    const char *output_path,
    char *staging_path,
    size_t staging_path_capacity,
    int *descriptor,
    int *error_number
) {
    const char *separator = strrchr(output_path, '/');
    if (separator == NULL || separator[1] == '\0') {
        *error_number = EINVAL;
        return false;
    }

    size_t directory_length = separator == output_path
        ? 1
        : (size_t)(separator - output_path);
    const char *suffix = directory_length == 1
        ? ".playdate-simctl-publish-XXXXXX"
        : "/.playdate-simctl-publish-XXXXXX";
    int staging_length = snprintf(
        staging_path,
        staging_path_capacity,
        "%.*s%s",
        (int)directory_length,
        output_path,
        suffix
    );
    if (
        staging_length < 0
        || (size_t)staging_length >= staging_path_capacity
    ) {
        *error_number = ENAMETOOLONG;
        return false;
    }

    *descriptor = mkstemp(staging_path);
    if (*descriptor < 0) {
        *error_number = errno;
        return false;
    }
    return true;
}

static bool pdsim_publish_across_filesystems(
    const char *temporary_path,
    const char *output_path,
    int *error_number
) {
    char staging_path[PATH_MAX];
    int staging_descriptor = -1;
    if (!pdsim_create_destination_staging_file(
            output_path,
            staging_path,
            sizeof(staging_path),
            &staging_descriptor,
            error_number
        )) {
        return false;
    }

    bool copied = pdsim_copy_file(temporary_path, staging_descriptor, error_number);
    if (close(staging_descriptor) != 0 && copied) {
        *error_number = errno;
        copied = false;
    }
    if (!copied) {
        (void)unlink(staging_path);
        return false;
    }

    if (
        renameatx_np(
            AT_FDCWD,
            staging_path,
            AT_FDCWD,
            output_path,
            RENAME_EXCL
        )
        != 0
    ) {
        int rename_error = errno;
        if (rename_error != EEXIST) {
            bool published = pdsim_publish_capture_by_exclusive_copy(
                staging_path,
                output_path,
                error_number
            );
            if (published) {
                (void)unlink(temporary_path);
                return true;
            }
        } else {
            *error_number = rename_error;
        }
        (void)unlink(staging_path);
        return false;
    }
    (void)unlink(temporary_path);
    return true;
}

bool pdsim_publish_capture_temporary_file(
    const char *temporary_path,
    const char *output_path,
    int *error_number
) {
    if (
        temporary_path == NULL || output_path == NULL || error_number == NULL
        || temporary_path[0] != '/' || output_path[0] != '/'
    ) {
        if (error_number != NULL) {
            *error_number = EINVAL;
        }
        return false;
    }

    if (link(temporary_path, output_path) == 0) {
        (void)unlink(temporary_path);
        return true;
    }
    if (errno == EEXIST) {
        *error_number = errno;
        return false;
    }

    if (
        renameatx_np(
            AT_FDCWD,
            temporary_path,
            AT_FDCWD,
            output_path,
            RENAME_EXCL
        )
        == 0
    ) {
        return true;
    }
    if (errno == EEXIST) {
        *error_number = errno;
        return false;
    }
    return pdsim_publish_across_filesystems(temporary_path, output_path, error_number);
}
