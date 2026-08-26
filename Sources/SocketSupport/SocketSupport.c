#include "SocketSupport.h"

#include "DescriptorIO.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

static void pdsim_set_message(
    char *error_message,
    size_t error_capacity,
    const char *message
) {
    if (error_message == NULL || error_capacity == 0) {
        return;
    }

    snprintf(error_message, error_capacity, "%s", message);
}

static void pdsim_set_errno_error(
    char *error_message,
    size_t error_capacity,
    const char *operation,
    int error_number
) {
    if (error_message == NULL || error_capacity == 0) {
        return;
    }

    char description[128];
    if (strerror_r(error_number, description, sizeof(description)) != 0) {
        snprintf(description, sizeof(description), "error %d", error_number);
    }
    snprintf(error_message, error_capacity, "%s: %s", operation, description);
}

static pdsim_socket_result pdsim_fail(
    pdsim_socket_result result,
    char *error_message,
    size_t error_capacity,
    const char *operation,
    int error_number
) {
    pdsim_set_errno_error(error_message, error_capacity, operation, error_number);
    errno = error_number;
    return result;
}

static pdsim_socket_result pdsim_fail_with_message(
    pdsim_socket_result result,
    char *error_message,
    size_t error_capacity,
    const char *message
) {
    pdsim_set_message(error_message, error_capacity, message);
    return result;
}

static bool pdsim_is_timeout_error(int error_number) {
    return error_number == EAGAIN || error_number == EWOULDBLOCK
        || error_number == ETIMEDOUT;
}

static int64_t pdsim_monotonic_milliseconds(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return -1;
    }
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int pdsim_wait_for_connection(int descriptor, uint32_t timeout_milliseconds) {
    int64_t start = pdsim_monotonic_milliseconds();
    if (start < 0) {
        return -1;
    }
    int64_t deadline = start + timeout_milliseconds;

    for (;;) {
        int64_t now = pdsim_monotonic_milliseconds();
        if (now < 0) {
            return -1;
        }
        int64_t remaining = deadline - now;
        if (remaining <= 0) {
            errno = ETIMEDOUT;
            return -1;
        }

        struct pollfd poll_descriptor = {
            .fd = descriptor,
            .events = POLLOUT,
        };
        int result = poll(&poll_descriptor, 1, (int)remaining);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (result == 0) {
            errno = ETIMEDOUT;
            return -1;
        }

        int socket_error = 0;
        socklen_t socket_error_length = sizeof(socket_error);
        if (
            getsockopt(
                descriptor,
                SOL_SOCKET,
                SO_ERROR,
                &socket_error,
                &socket_error_length
            )
            != 0
        ) {
            return -1;
        }
        if (socket_error != 0) {
            errno = socket_error;
            return -1;
        }
        return 0;
    }
}

static int pdsim_connect_with_timeout(
    int descriptor,
    const struct sockaddr_un *address,
    uint32_t timeout_milliseconds
) {
    int flags = fcntl(descriptor, F_GETFL);
    if (flags < 0 || fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) != 0) {
        return -1;
    }

    int result = connect(
        descriptor,
        (const struct sockaddr *)address,
        address->sun_len
    );
    if (
        result != 0 && errno != EINPROGRESS && errno != EAGAIN
        && errno != EWOULDBLOCK
    ) {
        return -1;
    }
    if (result != 0 && pdsim_wait_for_connection(descriptor, timeout_milliseconds) != 0) {
        return -1;
    }
    return fcntl(descriptor, F_SETFL, flags);
}

pdsim_socket_result pdsim_send_command(
    const char *socket_path,
    const char *command,
    pdsim_socket_configuration configuration,
    char *response,
    size_t response_capacity,
    char *error_message,
    size_t error_capacity
) {
    if (
        socket_path == NULL || command == NULL || response == NULL
        || response_capacity < 2 || error_message == NULL || error_capacity < 2
        || configuration.expected_peer_process_identifier <= 0
        || configuration.connect_timeout_milliseconds == 0
        || configuration.connect_timeout_milliseconds > INT_MAX
        || configuration.response_timeout_milliseconds == 0
    ) {
        return pdsim_fail_with_message(
            pdsim_socket_invalid_argument,
            error_message,
            error_capacity,
            "invalid socket arguments"
        );
    }

    response[0] = '\0';
    error_message[0] = '\0';

    size_t command_length = strlen(command);
    if (
        command_length == 0 || command_length > PDSIM_PROTOCOL_MAXIMUM_LINE_BYTES
        || memchr(command, '\n', command_length) != NULL
        || memchr(command, '\r', command_length) != NULL
    ) {
        return pdsim_fail_with_message(
            pdsim_socket_protocol_error,
            error_message,
            error_capacity,
            "invalid agent request framing"
        );
    }

    int descriptor = socket(AF_UNIX, SOCK_STREAM, 0);
    if (descriptor < 0) {
        return pdsim_fail(
            pdsim_socket_io_error,
            error_message,
            error_capacity,
            "socket",
            errno
        );
    }

    if (
        pdsim_configure_socket(
            descriptor,
            configuration.connect_timeout_milliseconds,
            configuration.response_timeout_milliseconds
        )
        != 0
    ) {
        int setup_error = errno;
        close(descriptor);
        return pdsim_fail(
            pdsim_socket_io_error,
            error_message,
            error_capacity,
            "configure socket",
            setup_error
        );
    }

    struct sockaddr_un address = {0};
    address.sun_family = AF_UNIX;
    size_t socket_path_length = strlcpy(
        address.sun_path,
        socket_path,
        sizeof(address.sun_path)
    );
    if (socket_path_length >= sizeof(address.sun_path)) {
        close(descriptor);
        return pdsim_fail(
            pdsim_socket_invalid_argument,
            error_message,
            error_capacity,
            "socket path",
            ENAMETOOLONG
        );
    }
    address.sun_len = (uint8_t)(offsetof(struct sockaddr_un, sun_path) + socket_path_length);

    if (
        pdsim_connect_with_timeout(
            descriptor,
            &address,
            configuration.connect_timeout_milliseconds
        )
        != 0
    ) {
        int connect_error = errno;
        close(descriptor);
        pdsim_socket_result result = pdsim_is_timeout_error(connect_error)
            ? pdsim_socket_timeout
            : (connect_error == ENOENT || connect_error == ECONNREFUSED
                    ? pdsim_socket_agent_not_running
                    : pdsim_socket_io_error);
        return pdsim_fail(
            result,
            error_message,
            error_capacity,
            "connect",
            connect_error
        );
    }

    pid_t peer_process_identifier = 0;
    socklen_t peer_process_identifier_length = sizeof(peer_process_identifier);
    if (
        getsockopt(
            descriptor,
            SOL_LOCAL,
            LOCAL_PEERPID,
            &peer_process_identifier,
            &peer_process_identifier_length
        )
        != 0
    ) {
        int peer_error = errno;
        close(descriptor);
        return pdsim_fail(
            pdsim_socket_io_error,
            error_message,
            error_capacity,
            "inspect agent peer",
            peer_error
        );
    }
    if (peer_process_identifier != configuration.expected_peer_process_identifier) {
        close(descriptor);
        return pdsim_fail_with_message(
            pdsim_socket_peer_mismatch,
            error_message,
            error_capacity,
            "the agent socket belongs to a different process"
        );
    }

    if (
        pdsim_write_line(descriptor, command, command_length) != 0
    ) {
        int write_error = errno;
        close(descriptor);
        return pdsim_fail(
            pdsim_is_timeout_error(write_error) ? pdsim_socket_timeout : pdsim_socket_io_error,
            error_message,
            error_capacity,
            "write",
            write_error
        );
    }
    if (shutdown(descriptor, SHUT_WR) != 0) {
        int shutdown_error = errno;
        close(descriptor);
        return pdsim_fail(
            pdsim_socket_io_error,
            error_message,
            error_capacity,
            "shutdown write side",
            shutdown_error
        );
    }

    size_t received = 0;
    pdsim_line_read_result read_result = pdsim_read_line(
        descriptor,
        response,
        response_capacity,
        &received
    );
    int read_error = errno;
    close(descriptor);

    if (read_result == pdsim_line_read_io_error) {
        return pdsim_fail(
            pdsim_is_timeout_error(read_error) ? pdsim_socket_timeout
                                                : pdsim_socket_io_error,
            error_message,
            error_capacity,
            "read",
            read_error
        );
    }

    if (received == 0) {
        return pdsim_fail_with_message(
            pdsim_socket_protocol_error,
            error_message,
            error_capacity,
            "empty agent response"
        );
    }
    if (read_result == pdsim_line_read_too_long) {
        return pdsim_fail_with_message(
            pdsim_socket_protocol_error,
            error_message,
            error_capacity,
            "agent response too large"
        );
    }
    if (read_result == pdsim_line_read_end_of_file) {
        return pdsim_fail_with_message(
            pdsim_socket_protocol_error,
            error_message,
            error_capacity,
            "unterminated agent response"
        );
    }
    if (
        memchr(response, '\r', received) != NULL
        || memchr(response, '\0', received) != NULL
    ) {
        return pdsim_fail_with_message(
            pdsim_socket_protocol_error,
            error_message,
            error_capacity,
            "invalid agent response framing"
        );
    }

    return pdsim_socket_success;
}
