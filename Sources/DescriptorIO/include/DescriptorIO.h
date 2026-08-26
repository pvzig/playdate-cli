#ifndef DESCRIPTOR_IO_H
#define DESCRIPTOR_IO_H

#include <lifetimebound.h>
#include <ptrcheck.h>
#include <stddef.h>
#include <sys/types.h>

typedef enum {
    pdsim_line_read_success = 0,
    pdsim_line_read_end_of_file,
    pdsim_line_read_too_long,
    pdsim_line_read_io_error,
} pdsim_line_read_result;

#pragma clang assume_nonnull begin

// Retries read(2) only when it is interrupted before transferring data.
ssize_t pdsim_read_retry(
    int descriptor,
    void * __sized_by(capacity) buffer __noescape,
    size_t capacity
);

// Writes the complete byte sequence or returns -1 with errno set. A zero-byte
// write before completion is treated as EIO.
int pdsim_write_all(
    int descriptor,
    const void * __sized_by(length) bytes __noescape,
    size_t length
);

// Writes the complete content followed by exactly one newline.
int pdsim_write_line(
    int descriptor,
    const char * __counted_by(length) bytes __noescape,
    size_t length
);

// Reads through the first newline, stores only its preceding content, and
// always NUL-terminates a nonempty buffer. End-of-file before a newline and a
// line that consumes capacity - 1 bytes have distinct results. Callers remain
// responsible for protocol-specific content validation.
pdsim_line_read_result pdsim_read_line(
    int descriptor,
    char * __counted_by(capacity) buffer __noescape,
    size_t capacity,
    size_t *line_length __noescape
);

#pragma clang assume_nonnull end

#endif
