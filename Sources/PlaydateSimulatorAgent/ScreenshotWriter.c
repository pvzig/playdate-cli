#include "ScreenshotWriter.h"

#include "CaptureFile.h"
#include "FramebufferImage.h"

#include <ImageIO/ImageIO.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void pdsim_set_error(
    char *error_message,
    size_t error_message_capacity,
    const char *message
) {
    if (error_message != NULL && error_message_capacity > 0) {
        snprintf(error_message, error_message_capacity, "%s", message);
    }
}

static void pdsim_set_errno_error(
    char *error_message,
    size_t error_message_capacity,
    const char *operation,
    int error_number
) {
    if (error_message != NULL && error_message_capacity > 0) {
        snprintf(
            error_message,
            error_message_capacity,
            "%s: %s",
            operation,
            strerror(error_number)
        );
    }
}

bool pdsim_write_screenshot_png(
    const uint8_t *framebuffer,
    const char *path,
    char *error_message,
    size_t error_message_capacity
) {
    if (framebuffer == NULL || path == NULL || path[0] != '/') {
        pdsim_set_error(error_message, error_message_capacity, "invalid screenshot input");
        return false;
    }

    CGImageRef image = pdsim_create_framebuffer_image(framebuffer);
    if (image == NULL) {
        pdsim_set_error(error_message, error_message_capacity, "could not create screenshot image");
        return false;
    }

    char temporary_path[PDSIM_CAPTURE_TEMPORARY_PATH_CAPACITY];
    int temporary_descriptor = pdsim_create_screenshot_temporary_file(
        temporary_path,
        sizeof(temporary_path)
    );
    if (temporary_descriptor < 0) {
        int creation_error = errno;
        CGImageRelease(image);
        pdsim_set_errno_error(
            error_message,
            error_message_capacity,
            "could not create temporary file",
            creation_error
        );
        return false;
    }
    close(temporary_descriptor);

    CFURLRef temporary_url = CFURLCreateFromFileSystemRepresentation(
        NULL,
        (const UInt8 *)temporary_path,
        (CFIndex)strlen(temporary_path),
        false
    );
    CGImageDestinationRef destination = temporary_url == NULL
        ? NULL
        : CGImageDestinationCreateWithURL(temporary_url, CFSTR("public.png"), 1, NULL);
    bool finalized = false;
    if (destination != NULL) {
        CGImageDestinationAddImage(destination, image, NULL);
        finalized = CGImageDestinationFinalize(destination);
        CFRelease(destination);
    }
    if (temporary_url != NULL) {
        CFRelease(temporary_url);
    }
    CGImageRelease(image);

    if (!finalized) {
        unlink(temporary_path);
        pdsim_set_error(error_message, error_message_capacity, "could not encode screenshot PNG");
        return false;
    }

    int save_error = 0;
    if (!pdsim_publish_capture_temporary_file(temporary_path, path, &save_error)) {
        if (error_message != NULL && error_message_capacity > 0) {
            snprintf(
                error_message,
                error_message_capacity,
                "could not save screenshot; temporary file preserved at %s: %s",
                temporary_path,
                strerror(save_error)
            );
        }
        return false;
    }
    return true;
}
