#include "DescriptorIO.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

ssize_t pdsim_read_retry(int descriptor, void *buffer, size_t capacity) {
    ssize_t result;
    do {
        result = read(descriptor, buffer, capacity);
    } while (result < 0 && errno == EINTR);
    return result;
}

int pdsim_write_all(int descriptor, const void *bytes, size_t length) {
    const unsigned char *cursor = bytes;
    size_t written = 0;
    while (written < length) {
        ssize_t result = write(descriptor, cursor + written, length - written);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (result == 0) {
            errno = EIO;
            return -1;
        }
        written += (size_t)result;
    }
    return 0;
}

int pdsim_write_line(int descriptor, const char *bytes, size_t length) {
    if (pdsim_write_all(descriptor, bytes, length) != 0) {
        return -1;
    }
    return pdsim_write_all(descriptor, "\n", 1);
}

pdsim_line_read_result pdsim_read_line(
    int descriptor,
    char *buffer,
    size_t capacity,
    size_t *line_length
) {
    if (capacity < 2) {
        errno = EINVAL;
        return pdsim_line_read_io_error;
    }

    size_t received = 0;
    buffer[0] = '\0';
    *line_length = 0;
    while (received < capacity - 1) {
        ssize_t result = pdsim_read_retry(
            descriptor,
            buffer + received,
            capacity - 1 - received
        );
        if (result < 0) {
            buffer[received] = '\0';
            *line_length = received;
            return pdsim_line_read_io_error;
        }
        if (result == 0) {
            buffer[received] = '\0';
            *line_length = received;
            return pdsim_line_read_end_of_file;
        }

        char *newline = memchr(buffer + received, '\n', (size_t)result);
        received += (size_t)result;
        if (newline != NULL) {
            size_t content_length = (size_t)(newline - buffer);
            buffer[content_length] = '\0';
            *line_length = content_length;
            return pdsim_line_read_success;
        }
    }

    buffer[received] = '\0';
    *line_length = received;
    return pdsim_line_read_too_long;
}
